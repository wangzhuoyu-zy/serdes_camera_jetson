/*
 * fzcam_sync.c - Independent TSC sync kernel module for Jetson AGX Orin
 *
 * Provides TSC-based PWM trigger signal generation and timestamp capture.
 * Exposes /dev/fzcam_sync via MISC device with ioctl interface.
 *
 * Compatible: "fz,fzcam_sync"
 * Platform:   NVIDIA Jetson AGX Orin, JetPack 6.2
 * License:    GPL v2
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/debugfs.h>
#include <asm/ioctl.h>

/* ---- Module parameters ---- */
static int debug_en;
module_param(debug_en, int, 0644);
MODULE_PARM_DESC(debug_en, "Enable debug messages (0/1)");

/* ---- Constants ---- */
#define FZCAM_SYNC_MAX_TS_QUEUE   32
#define FZCAM_SYNC_FPS_SCALE      100
#define FZCAM_SYNC_TSC_FPS_SCALE  1000

#define FZCAM_SYNC_FLAG_VALID     BIT(0)
#define FZCAM_SYNC_FLAG_OVERFLOW  BIT(1)

#define TSC_TICKS_PER_HZ          (31250000ULL)
#define TSC_NS_PER_TICK           (32)
#define NS_PER_MS                 (1000000U)

#define TSC_GEN_START_OFFSET_MS   (100)

/* ---- TSC Register offsets ---- */
#define TSC_MTSCCNTCV0            (0x10)
#define TSC_MTSCCNTCV1            (0x14)

#define TSC_GENX_CTRL             (0x00)
#define TSC_GENX_CTRL_RST         (0x00)
#define TSC_GENX_CTRL_INITIAL_VAL BIT(1)
#define TSC_GENX_CTRL_ENABLE      BIT(0)

#define TSC_GENX_START0           (0x04)
#define TSC_GENX_START0_LSB       GENMASK(31, 0)

#define TSC_GENX_START1           (0x08)
#define TSC_GENX_START1_MSB       GENMASK(23, 0)

#define TSC_GENX_STATUS           (0x0C)
#define TSC_GENX_STATUS_RUNNING   BIT(1)
#define TSC_GENX_STATUS_WAITING   BIT(0)

#define TSC_GENX_EDGE0            (0x18)
#define TSC_GENX_EDGE1            (0x1C)

#define TSC_GENX_EDGEX_INTERRUPT_EN BIT(31)
#define TSC_GENX_EDGEX_STOP       BIT(30)
#define TSC_GENX_EDGEX_TOGGLE     BIT(29)
#define TSC_GENX_EDGEX_LOOP       BIT(28)
#define TSC_GENX_EDGEX_OFFSET     GENMASK(27, 0)

/* ---- ioctl Commands ---- */
#define FZCAM_SYNC_START          _IOW('f', 1, fzcam_sync_config_t)
#define FZCAM_SYNC_STOP           _IO('f', 2)
#define FZCAM_SYNC_GET_TS         _IOR('f', 3, fzcam_sync_ts_t)
#define FZCAM_SYNC_GET_TS_BATCH   _IOR('f', 4, fzcam_sync_ts_batch_t)
#define FZCAM_SYNC_CLEAR_TS       _IO('f', 5)
#define FZCAM_SYNC_GET_STATUS     _IOR('f', 6, fzcam_sync_status_t)

/* ---- Types (must match userspace fzcam_sync_ioctl.h) ---- */
typedef struct {
    uint64_t tsc_counter_ns;
    uint64_t capture_timestamp_ns;
    uint32_t generator_id;
    uint32_t edge_id;
    uint32_t sequence;
    uint32_t flags;
} fzcam_sync_ts_t;

typedef struct {
    fzcam_sync_ts_t timestamps[FZCAM_SYNC_MAX_TS_QUEUE];
    uint32_t count;
    uint32_t flags;
} fzcam_sync_ts_batch_t;

typedef struct {
    uint16_t fps;
    uint16_t duty_cycle;
    uint32_t enable_timestamp;
} fzcam_sync_config_t;

typedef struct {
    uint32_t running;
    uint32_t ts_queue_count;
    uint32_t ts_sequence;
    uint32_t generator_count;
} fzcam_sync_status_t;

/* ---- Ring buffer for timestamps ---- */
typedef struct {
    fzcam_sync_ts_t entries[FZCAM_SYNC_MAX_TS_QUEUE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    spinlock_t lock;
} fzcam_sync_ts_queue_t;

/* ---- Per-generator context ---- */
struct fzcam_sync_gen {
    void __iomem *base;
    struct device_node *of;
    u32 freq_hz;
    u32 duty_cycle;
    u32 offset_ms;
    u32 id;
    bool ts_enabled;
    struct list_head list;
};

/* ---- Main device context ---- */
struct fzcam_sync_dev {
    struct device *dev;
    void __iomem *tsc_base;
    struct list_head generators;
    fzcam_sync_ts_queue_t ts_queue;
    u32 ts_sequence;
    u32 gen_count;
    struct hrtimer ts_capture_timer;
    bool ts_capture_active;
    bool running;
    unsigned long period_ns;
    struct dentry *debugfs_dir;
};

static struct fzcam_sync_dev *fzcam_sync;

/* ---- Register access helpers ---- */
static inline void gen_writel(struct fzcam_sync_gen *gen, u32 reg, u32 val)
{
    writel(val, gen->base + reg);
}

static inline u32 gen_readl(struct fzcam_sync_gen *gen, u32 reg)
{
    return readl(gen->base + reg);
}

static inline u32 ctrl_readl(u32 reg)
{
    return readl(fzcam_sync->tsc_base + reg);
}

static inline u64 tsc_read_counter(void)
{
    u32 cv0 = ctrl_readl(TSC_MTSCCNTCV0);
    u32 cv1 = ctrl_readl(TSC_MTSCCNTCV1);
    return ((u64)cv1 << 32) | cv0;
}

static inline u64 tsc_counter_to_ns(u64 counter)
{
    return counter * TSC_NS_PER_TICK;
}

/* ---- Timestamp queue operations ---- */
static void ts_queue_init(fzcam_sync_ts_queue_t *q)
{
    memset(q, 0, sizeof(*q));
    spin_lock_init(&q->lock);
}

static void ts_queue_push(fzcam_sync_ts_queue_t *q, fzcam_sync_ts_t *ts)
{
    unsigned long flags;

    spin_lock_irqsave(&q->lock, flags);
    memcpy(&q->entries[q->head], ts, sizeof(*ts));
    q->head = (q->head + 1) % FZCAM_SYNC_MAX_TS_QUEUE;
    if (q->count < FZCAM_SYNC_MAX_TS_QUEUE)
        q->count++;
    else
        q->tail = (q->tail + 1) % FZCAM_SYNC_MAX_TS_QUEUE;
    spin_unlock_irqrestore(&q->lock, flags);
}

static int ts_queue_pop(fzcam_sync_ts_queue_t *q, fzcam_sync_ts_t *ts)
{
    unsigned long flags;
    int ret = -ENODATA;

    spin_lock_irqsave(&q->lock, flags);
    if (q->count > 0) {
        memcpy(ts, &q->entries[q->tail], sizeof(*ts));
        q->tail = (q->tail + 1) % FZCAM_SYNC_MAX_TS_QUEUE;
        q->count--;
        ret = 0;
    }
    spin_unlock_irqrestore(&q->lock, flags);
    return ret;
}

static void ts_queue_clear(fzcam_sync_ts_queue_t *q)
{
    unsigned long flags;

    spin_lock_irqsave(&q->lock, flags);
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    spin_unlock_irqrestore(&q->lock, flags);
}

/* ---- Generator management ---- */
static bool gen_is_running(struct fzcam_sync_gen *gen)
{
    return (gen_readl(gen, TSC_GENX_STATUS) & TSC_GENX_STATUS_RUNNING) != 0;
}

static bool gen_is_waiting(struct fzcam_sync_gen *gen)
{
    return (gen_readl(gen, TSC_GENX_STATUS) & TSC_GENX_STATUS_WAITING) != 0;
}

static bool gen_is_idle(struct fzcam_sync_gen *gen)
{
    return !gen_is_running(gen) && !gen_is_waiting(gen);
}

static int program_generator_edges(struct fzcam_sync_gen *gen)
{
    u32 ticks_in_period, ticks_active, ticks_inactive;

    ticks_in_period = DIV_ROUND_CLOSEST(
        TSC_TICKS_PER_HZ * FZCAM_SYNC_TSC_FPS_SCALE, gen->freq_hz);

    ticks_active = ticks_in_period * gen->duty_cycle / 100;
    ticks_inactive = ticks_in_period - ticks_active;

    /* EDGE0: rising edge → toggle after active duration */
    gen_writel(gen, TSC_GENX_EDGE0,
        TSC_GENX_EDGEX_TOGGLE |
        FIELD_PREP(TSC_GENX_EDGEX_OFFSET, ticks_active));

    /* EDGE1: falling edge → toggle + loop back after inactive duration */
    gen_writel(gen, TSC_GENX_EDGE1,
        TSC_GENX_EDGEX_TOGGLE |
        TSC_GENX_EDGEX_LOOP |
        FIELD_PREP(TSC_GENX_EDGEX_OFFSET, ticks_inactive));

    return 0;
}

static void program_generator_start_values(struct fzcam_sync_gen *gen)
{
    u32 relative_ticks = TSC_GEN_START_OFFSET_MS * NS_PER_MS / TSC_NS_PER_TICK;
    u64 current_ticks = tsc_read_counter();
    u64 start_ticks = current_ticks + relative_ticks;

    if (gen->offset_ms != 0)
        start_ticks += (u64)gen->offset_ms * NS_PER_MS / TSC_NS_PER_TICK;

    gen_writel(gen, TSC_GENX_START0,
        FIELD_PREP(TSC_GENX_START0_LSB, lower_32_bits(start_ticks)));
    gen_writel(gen, TSC_GENX_START1,
        FIELD_PREP(TSC_GENX_START1_MSB, upper_32_bits(start_ticks)));
}

static int start_generators(void)
{
    struct fzcam_sync_gen *gen;

    dev_info(fzcam_sync->dev, "Starting TSC generators...\n");

    list_for_each_entry(gen, &fzcam_sync->generators, list) {
        if (!gen_is_idle(gen)) {
            dev_err(fzcam_sync->dev, "Generator %u is not idle\n", gen->id);
            return -EBUSY;
        }
    }

    list_for_each_entry(gen, &fzcam_sync->generators, list) {
        u32 ctrl;

        program_generator_edges(gen);
        program_generator_start_values(gen);

        ctrl = TSC_GENX_CTRL_INITIAL_VAL | TSC_GENX_CTRL_ENABLE;

        /* Enable EDGE0 interrupt for timestamp if requested */
        if (gen->ts_enabled) {
            u32 edge0 = gen_readl(gen, TSC_GENX_EDGE0);
            edge0 |= TSC_GENX_EDGEX_INTERRUPT_EN;
            gen_writel(gen, TSC_GENX_EDGE0, edge0);
            dev_info(fzcam_sync->dev, "Generator %u: timestamp interrupt enabled\n",
                gen->id);
        }

        gen_writel(gen, TSC_GENX_CTRL, ctrl);
        dev_dbg(fzcam_sync->dev, "Generator %u started\n", gen->id);
    }

    fzcam_sync->running = true;
    return 0;
}

static void stop_generators(void)
{
    struct fzcam_sync_gen *gen;

    dev_info(fzcam_sync->dev, "Stopping TSC generators...\n");

    list_for_each_entry(gen, &fzcam_sync->generators, list) {
        gen_writel(gen, TSC_GENX_CTRL, TSC_GENX_CTRL_RST);
        dev_dbg(fzcam_sync->dev, "Generator %u stopped\n", gen->id);
    }

    fzcam_sync->running = false;
}

/* ---- hrtimer timestamp capture callback ----
 *
 * Fires at each PWM period. Captures CLOCK_MONOTONIC time + TSC counter,
 * and pushes the pair into the timestamp ring buffer.
 * This is the V2.5 approach: raw capture without cross-domain conversion.
 */
static enum hrtimer_restart ts_capture_cb(struct hrtimer *timer)
{
    struct fzcam_sync_gen *gen;
    fzcam_sync_ts_t ts;

    if (!fzcam_sync->ts_capture_active)
        return HRTIMER_NORESTART;

    ts.capture_timestamp_ns = ktime_get_ns();
    ts.tsc_counter_ns = tsc_counter_to_ns(tsc_read_counter());

    /* Assign to first ts_enabled generator */
    list_for_each_entry(gen, &fzcam_sync->generators, list) {
        if (gen->ts_enabled) {
            ts.generator_id = gen->id;
            ts.edge_id = 0;
            ts.sequence = fzcam_sync->ts_sequence++;
            ts.flags = FZCAM_SYNC_FLAG_VALID;
            ts_queue_push(&fzcam_sync->ts_queue, &ts);
            break;
        }
    }

    if (debug_en)
        dev_dbg(fzcam_sync->dev,
            "TS: gen=%u seq=%u tsc=%llu cap=%llu\n",
            ts.generator_id, ts.sequence,
            ts.tsc_counter_ns, ts.capture_timestamp_ns);

    hrtimer_forward_now(timer, ns_to_ktime(fzcam_sync->period_ns));
    return HRTIMER_RESTART;
}

static int start_ts_capture(unsigned long period_ns)
{
    ktime_t kt;

    fzcam_sync->period_ns = period_ns;
    fzcam_sync->ts_capture_active = true;

    /* Fire after one period delay to let generators stabilize */
    kt = ktime_set(0, period_ns);
    hrtimer_start(&fzcam_sync->ts_capture_timer, kt,
                  HRTIMER_MODE_REL_PINNED_HARD);

    dev_info(fzcam_sync->dev, "Timestamp capture started, period=%luns\n",
        period_ns);
    return 0;
}

static void stop_ts_capture(void)
{
    fzcam_sync->ts_capture_active = false;
    hrtimer_cancel(&fzcam_sync->ts_capture_timer);
    dev_info(fzcam_sync->dev, "Timestamp capture stopped\n");
}

static unsigned long calc_period_ns(u16 fps)
{
    /* fps is in FPS_SCALE units (×100), e.g. 3000 = 30.00 Hz */
    if (fps == 0) fps = 3000;
    return 1000000000UL * FZCAM_SYNC_FPS_SCALE / fps;
}

/*
 * Recalculate generator freq_hz based on desired FPS + system offset compensation.
 * This mirrors the fzcam_sync approach for fine-tuning PWM period.
 */
static int adjust_gen_freq_for_fps(u16 fps)
{
    struct fzcam_sync_gen *gen;
    int offset_us = 50; /* Default hwtimer offset in microseconds */

    /* Select offset based on FPS (tuned values from fzcam_sync) */
    switch (fps) {
    case 6000: offset_us = 30; break;
    case 3000: offset_us = 50; break;
    case 2000: offset_us = 65; break;
    case 1500:
    case 1000:
    case 600:
    case 500:  offset_us = 50; break;
    default:   offset_us = 100; break;
    }

    list_for_each_entry(gen, &fzcam_sync->generators, list) {
        gen->freq_hz = (1000000UL * FZCAM_SYNC_TSC_FPS_SCALE) /
                       (1000000UL * FZCAM_SYNC_FPS_SCALE / fps + offset_us);
        dev_info(fzcam_sync->dev, "Generator %u: freq_hz=%u (target %u.%02u Hz)\n",
            gen->id, gen->freq_hz, fps / 100, fps % 100);
    }

    return 0;
}

/* ---- Device tree: discover and add generators ---- */
static int discover_generators(struct device *dev)
{
    struct device_node *np;
    struct fzcam_sync_gen *gen;
    struct resource res;
    const char *status;
    int err;

    for_each_child_of_node(dev->of_node, np) {
        err = of_property_read_string(np, "status", &status);
        if (err == 0 && strcmp("okay", status) != 0) {
            dev_dbg(dev, "Generator %s disabled, skipping\n", np->full_name);
            continue;
        }

        gen = devm_kzalloc(dev, sizeof(*gen), GFP_KERNEL);
        if (!gen)
            return -ENOMEM;

        gen->of = np;
        INIT_LIST_HEAD(&gen->list);

        if (of_address_to_resource(np, 0, &res))
            return -EINVAL;

        gen->base = devm_ioremap_resource(dev, &res);
        if (IS_ERR(gen->base))
            return PTR_ERR(gen->base);

        err = of_property_read_u32(np, "freq_hz", &gen->freq_hz);
        if (err) {
            dev_err(dev, "Failed to read freq_hz: %d\n", err);
            return err;
        }
        gen->freq_hz *= FZCAM_SYNC_TSC_FPS_SCALE;
        if (gen->freq_hz == 0) {
            dev_err(dev, "freq_hz must be non-zero\n");
            return -EINVAL;
        }

        err = of_property_read_u32(np, "duty_cycle", &gen->duty_cycle);
        if (err) {
            dev_err(dev, "Failed to read duty_cycle: %d\n", err);
            return err;
        }
        if (gen->duty_cycle >= 100) {
            dev_err(dev, "duty_cycle must be < 100\n");
            return -EINVAL;
        }

        err = of_property_read_u32(np, "offset_ms", &gen->offset_ms);
        if (err)
            gen->offset_ms = 0;

        err = of_property_read_u32(np, "generator-id", &gen->id);
        if (err)
            gen->id = 0;

        gen->ts_enabled = of_property_read_bool(np, "timestamp-enabled");

        list_add_tail(&gen->list, &fzcam_sync->generators);
        fzcam_sync->gen_count++;

        dev_info(dev, "Generator %u: freq=%u duty=%u%% ts=%s\n",
            gen->id, gen->freq_hz, gen->duty_cycle,
            gen->ts_enabled ? "enabled" : "disabled");
    }

    return 0;
}

/* ---- fops: open / release ---- */
static int fzcam_sync_open(struct inode *inode, struct file *file)
{
    dev_info(fzcam_sync->dev, "Device opened\n");
    return 0;
}

static int fzcam_sync_release(struct inode *inode, struct file *file)
{
    dev_info(fzcam_sync->dev, "Device closed\n");
    if (fzcam_sync->running) {
        stop_ts_capture();
        stop_generators();
    }
    return 0;
}

/* ---- fops: ioctl ---- */
static long fzcam_sync_ioctl(struct file *file, unsigned int cmd,
                             unsigned long arg)
{
    int ret;

    switch (cmd) {
    case FZCAM_SYNC_START: {
        fzcam_sync_config_t cfg;

        ret = copy_from_user(&cfg, (void *)arg, sizeof(cfg));
        if (ret) {
            dev_err(fzcam_sync->dev, "copy_from_user failed\n");
            return ret;
        }

        if (cfg.fps < 100 || cfg.fps > 12000) {
            dev_err(fzcam_sync->dev, "fps out of range (1.00-120.00): %u\n",
                cfg.fps);
            return -EINVAL;
        }

        /* Apply duty_cycle from config if provided */
        if (cfg.duty_cycle > 0 && cfg.duty_cycle < 100) {
            struct fzcam_sync_gen *gen;
            list_for_each_entry(gen, &fzcam_sync->generators, list)
                gen->duty_cycle = cfg.duty_cycle;
        }

        /* Enable timestamp for all generators if requested */
        if (cfg.enable_timestamp) {
            struct fzcam_sync_gen *gen;
            list_for_each_entry(gen, &fzcam_sync->generators, list)
                gen->ts_enabled = true;
        }

        /* Recalculate freq for target FPS and start generators */
        adjust_gen_freq_for_fps(cfg.fps);
        ret = start_generators();
        if (ret)
            return ret;

        /* Start hrtimer-based timestamp capture */
        ret = start_ts_capture(calc_period_ns(cfg.fps));
        if (ret) {
            stop_generators();
            return ret;
        }

        dev_info(fzcam_sync->dev,
            "Started: fps=%u.%02u duty=%u%% ts=%s\n",
            cfg.fps / 100, cfg.fps % 100, cfg.duty_cycle,
            cfg.enable_timestamp ? "on" : "off");
        break;
    }

    case FZCAM_SYNC_STOP:
        stop_ts_capture();
        stop_generators();
        dev_info(fzcam_sync->dev, "Stopped\n");
        break;

    case FZCAM_SYNC_GET_TS: {
        fzcam_sync_ts_t ts;

        ret = ts_queue_pop(&fzcam_sync->ts_queue, &ts);
        if (ret == -ENODATA)
            return -EAGAIN;
        if (ret == 0) {
            ret = copy_to_user((void *)arg, &ts, sizeof(ts));
            if (ret)
                dev_err(fzcam_sync->dev, "copy_to_user ts failed\n");
        }
        break;
    }

    case FZCAM_SYNC_GET_TS_BATCH: {
        fzcam_sync_ts_batch_t batch;
        uint32_t i = 0;

        memset(&batch, 0, sizeof(batch));
        while (i < FZCAM_SYNC_MAX_TS_QUEUE) {
            ret = ts_queue_pop(&fzcam_sync->ts_queue, &batch.timestamps[i]);
            if (ret == -ENODATA)
                break;
            if (ret != 0)
                return ret;
            i++;
        }
        batch.count = i;

        ret = copy_to_user((void *)arg, &batch, sizeof(batch));
        if (ret)
            dev_err(fzcam_sync->dev, "copy_to_user batch failed\n");
        break;
    }

    case FZCAM_SYNC_CLEAR_TS:
        ts_queue_clear(&fzcam_sync->ts_queue);
        dev_info(fzcam_sync->dev, "Timestamp queue cleared\n");
        break;

    case FZCAM_SYNC_GET_STATUS: {
        fzcam_sync_status_t st;

        memset(&st, 0, sizeof(st));
        st.running = fzcam_sync->running ? 1 : 0;
        st.ts_queue_count = fzcam_sync->ts_queue.count;
        st.ts_sequence = fzcam_sync->ts_sequence;
        st.generator_count = fzcam_sync->gen_count;

        ret = copy_to_user((void *)arg, &st, sizeof(st));
        if (ret)
            dev_err(fzcam_sync->dev, "copy_to_user status failed\n");
        break;
    }

    default:
        return -ENOTTY;
    }

    return 0;
}

static const struct file_operations fzcam_sync_fops = {
    .owner          = THIS_MODULE,
    .llseek         = no_llseek,
    .unlocked_ioctl = fzcam_sync_ioctl,
    .open           = fzcam_sync_open,
    .release        = fzcam_sync_release,
};

static struct miscdevice fzcam_sync_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "fzcam_sync",
    .fops  = &fzcam_sync_fops,
};

/* ---- debugfs ---- */
#ifdef CONFIG_DEBUG_FS

static int dbg_ts_count_show(struct seq_file *s, void *data)
{
    seq_printf(s, "%u\n", fzcam_sync->ts_queue.count);
    return 0;
}

static int dbg_ts_latest_show(struct seq_file *s, void *data)
{
    unsigned long flags;
    uint32_t idx;
    fzcam_sync_ts_t *ts;

    spin_lock_irqsave(&fzcam_sync->ts_queue.lock, flags);
    if (fzcam_sync->ts_queue.count > 0) {
        idx = (fzcam_sync->ts_queue.head + FZCAM_SYNC_MAX_TS_QUEUE - 1)
              % FZCAM_SYNC_MAX_TS_QUEUE;
        ts = &fzcam_sync->ts_queue.entries[idx];
        seq_printf(s, "gen=%u edge=%u seq=%u tsc=%llu capture=%llu flags=0x%x\n",
            ts->generator_id, ts->edge_id, ts->sequence,
            ts->tsc_counter_ns, ts->capture_timestamp_ns, ts->flags);
    } else {
        seq_puts(s, "empty\n");
    }
    spin_unlock_irqrestore(&fzcam_sync->ts_queue.lock, flags);
    return 0;
}

static int dbg_status_show(struct seq_file *s, void *data)
{
    seq_printf(s, "running:      %s\n", fzcam_sync->running ? "yes" : "no");
    seq_printf(s, "ts_capture:   %s\n", fzcam_sync->ts_capture_active ? "active" : "stopped");
    seq_printf(s, "ts_sequence:  %u\n", fzcam_sync->ts_sequence);
    seq_printf(s, "ts_queue:     %u/%u\n", fzcam_sync->ts_queue.count,
        FZCAM_SYNC_MAX_TS_QUEUE);
    seq_printf(s, "generators:   %u\n", fzcam_sync->gen_count);
    seq_printf(s, "period_ns:    %lu\n", fzcam_sync->period_ns);
    return 0;
}

static int dbg_ts_count_open(struct inode *i, struct file *f)
{ return single_open(f, dbg_ts_count_show, NULL); }

static int dbg_ts_latest_open(struct inode *i, struct file *f)
{ return single_open(f, dbg_ts_latest_show, NULL); }

static int dbg_status_open(struct inode *i, struct file *f)
{ return single_open(f, dbg_status_show, NULL); }

static const struct file_operations dbg_ts_count_fops = {
    .owner = THIS_MODULE, .open = dbg_ts_count_open,
    .read = seq_read, .llseek = seq_lseek, .release = single_release,
};
static const struct file_operations dbg_ts_latest_fops = {
    .owner = THIS_MODULE, .open = dbg_ts_latest_open,
    .read = seq_read, .llseek = seq_lseek, .release = single_release,
};
static const struct file_operations dbg_status_fops = {
    .owner = THIS_MODULE, .open = dbg_status_open,
    .read = seq_read, .llseek = seq_lseek, .release = single_release,
};

static int fzcam_sync_debugfs_init(void)
{
    struct dentry *ts_dir;

    fzcam_sync->debugfs_dir = debugfs_create_dir("fzcam_sync", NULL);
    if (IS_ERR(fzcam_sync->debugfs_dir))
        return PTR_ERR(fzcam_sync->debugfs_dir);

    ts_dir = debugfs_create_dir("timestamp", fzcam_sync->debugfs_dir);
    if (IS_ERR(ts_dir))
        return PTR_ERR(ts_dir);

    debugfs_create_file("count", 0444, ts_dir, NULL, &dbg_ts_count_fops);
    debugfs_create_file("latest", 0444, ts_dir, NULL, &dbg_ts_latest_fops);
    debugfs_create_file("status", 0444, fzcam_sync->debugfs_dir, NULL,
        &dbg_status_fops);

    return 0;
}

static void fzcam_sync_debugfs_remove(void)
{
    debugfs_remove_recursive(fzcam_sync->debugfs_dir);
    fzcam_sync->debugfs_dir = NULL;
}
#else
static inline int fzcam_sync_debugfs_init(void) { return 0; }
static inline void fzcam_sync_debugfs_remove(void) {}
#endif

/* ---- platform driver probe / remove ---- */
static int fzcam_sync_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct resource *res;
    int err;

    fzcam_sync = devm_kzalloc(dev, sizeof(*fzcam_sync), GFP_KERNEL);
    if (!fzcam_sync)
        return -ENOMEM;

    fzcam_sync->dev = dev;
    INIT_LIST_HEAD(&fzcam_sync->generators);
    ts_queue_init(&fzcam_sync->ts_queue);

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    fzcam_sync->tsc_base = devm_ioremap_resource(dev, res);
    if (IS_ERR(fzcam_sync->tsc_base))
        return PTR_ERR(fzcam_sync->tsc_base);

    err = discover_generators(dev);
    if (err)
        return err;

    /* Init timestamp capture hrtimer */
    hrtimer_init(&fzcam_sync->ts_capture_timer, CLOCK_MONOTONIC,
                 HRTIMER_MODE_REL_PINNED_HARD);
    fzcam_sync->ts_capture_timer.function = ts_capture_cb;

    err = fzcam_sync_debugfs_init();
    if (err)
        dev_warn(dev, "debugfs init failed: %d\n", err);

    err = misc_register(&fzcam_sync_misc);
    if (err) {
        dev_err(dev, "misc_register failed: %d\n", err);
        fzcam_sync_debugfs_remove();
        return err;
    }

    dev_info(dev, "fzcam_sync probe success, /dev/fzcam_sync ready\n");
    return 0;
}

static int fzcam_sync_remove(struct platform_device *pdev)
{
    if (fzcam_sync->running) {
        stop_ts_capture();
        stop_generators();
    }
    misc_deregister(&fzcam_sync_misc);
    fzcam_sync_debugfs_remove();
    dev_info(&pdev->dev, "fzcam_sync removed\n");
    return 0;
}

static const struct of_device_id fzcam_sync_of_match[] = {
    { .compatible = "fz,fzcam_sync" },
    {},
};
MODULE_DEVICE_TABLE(of, fzcam_sync_of_match);

static struct platform_driver fzcam_sync_driver = {
    .probe  = fzcam_sync_probe,
    .remove = fzcam_sync_remove,
    .driver = {
        .name           = "fzcam_sync",
        .of_match_table = fzcam_sync_of_match,
    },
};

module_platform_driver(fzcam_sync_driver);

MODULE_AUTHOR("fzcam_sync team");
MODULE_DESCRIPTION("fzcam_sync - TSC trigger signal output and timestamp capture");
MODULE_LICENSE("GPL");
