/*
 * fzcam_sync_daemon.c - User-space control daemon for fzcam_sync.ko
 *
 * Reads /etc/fzcam_sync/fzcam_sync.conf for FPS/ duty_cycle configuration,
 * starts TSC PWM output with timestamp capture, and continuously reads
 * and prints timestamps from /dev/fzcam_sync.
 *
 * Usage:
 *   fzcam_sync_daemon [config_file]
 *
 * Signals: SIGTERM / SIGINT for graceful shutdown.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

/* ---- Configuration defaults ---- */
#define DEFAULT_FPS         30000    /* 30.00 Hz */
#define DEFAULT_DUTY_CYCLE  10      /* 10% */
#define DEFAULT_CONF_FILE   "/etc/fzcam_sync/fzcam_sync.conf"
#define DEV_NODE             "/dev/fzcam_sync"

/* ---- ioctl definitions (must match kernel module) ---- */
#define FZCAM_SYNC_MAX_TS_QUEUE  32
#define FZCAM_SYNC_FPS_SCALE     100

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

#define FZCAM_SYNC_START          _IOW('f', 1, fzcam_sync_config_t)
#define FZCAM_SYNC_STOP           _IO('f', 2)
#define FZCAM_SYNC_GET_TS         _IOR('f', 3, fzcam_sync_ts_t)
#define FZCAM_SYNC_GET_TS_BATCH   _IOR('f', 4, fzcam_sync_ts_batch_t)
#define FZCAM_SYNC_CLEAR_TS       _IO('f', 5)
#define FZCAM_SYNC_GET_STATUS     _IOR('f', 6, fzcam_sync_status_t)

/* ---- Global state ---- */
static volatile int running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

/* ---- Configuration file parser ---- */
static int read_config(const char *conf_path, int *fps, int *duty_cycle)
{
    FILE *fp;
    char line[256];

    *fps = DEFAULT_FPS;
    *duty_cycle = DEFAULT_DUTY_CYCLE;

    fp = fopen(conf_path, "r");
    if (!fp) {
        fprintf(stderr, "fzcam_sync_daemon: cannot open %s, using defaults\n",
                conf_path);
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        char *p = line;

        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\t') p++;

        /* Skip comments and blank lines */
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;

        if (strncasecmp(p, "FPS=", 4) == 0) {
            int val = atoi(p + 4);
            if (val >= 100 && val <= 9000)
                *fps = val;
        } else if (strncasecmp(p, "DUTY_CYCLE=", 11) == 0) {
            int val = atoi(p + 11);
            if (val >= 1 && val <= 99)
                *duty_cycle = val;
        }
    }

    fclose(fp);
    return 0;
}

/* ---- Main ---- */
int main(int argc, char **argv)
{
    int fd;
    int fps, duty_cycle;
    const char *conf_path;
    fzcam_sync_config_t cfg;
    fzcam_sync_status_t status;
    int ret;

    /* Parse arguments */
    conf_path = (argc > 1) ? argv[1] : DEFAULT_CONF_FILE;

    /* Read configuration */
    read_config(conf_path, &fps, &duty_cycle);

    fprintf(stderr, "fzcam_sync_daemon: starting\n");
    fprintf(stderr, "  config:    %s\n", conf_path);
    fprintf(stderr, "  FPS:       %d (%.2f Hz)\n", fps, fps / 100.0);
    fprintf(stderr, "  duty_cycle: %d%%\n", duty_cycle);

    /* Open device */
    fd = open(DEV_NODE, O_RDWR);
    if (fd < 0) {
        perror("fzcam_sync_daemon: failed to open " DEV_NODE);
        return 1;
    }

    /* Configure and start TSC output with timestamp */
    memset(&cfg, 0, sizeof(cfg));
    cfg.fps = (uint16_t)fps;
    cfg.duty_cycle = (uint16_t)duty_cycle;
    cfg.enable_timestamp = 1;

    if (ioctl(fd, FZCAM_SYNC_START, &cfg) < 0) {
        perror("fzcam_sync_daemon: FZCAM_SYNC_START failed");
        close(fd);
        return 1;
    }

    fprintf(stderr, "fzcam_sync_daemon: TSC started, reading timestamps...\n");

    /* Get status */
    if (ioctl(fd, FZCAM_SYNC_GET_STATUS, &status) == 0) {
        fprintf(stderr, "  generators: %u\n", status.generator_count);
    }

    /* Setup signal handlers for graceful shutdown */
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    /* Main loop: read and print timestamps */
    while (running) {
        fzcam_sync_ts_batch_t batch;
        uint32_t i;

        ret = ioctl(fd, FZCAM_SYNC_GET_TS_BATCH, &batch);
        if (ret < 0) {
            if (errno == EAGAIN) {
                /* No data yet, wait and retry */
                usleep(100000); /* 100ms */
                continue;
            }
            perror("fzcam_sync_daemon: FZCAM_SYNC_GET_TS_BATCH failed");
            break;
        }

        if (batch.count == 0) {
            /* No new timestamps, sleep briefly */
            sleep(1);
            continue;
        }

        /* Print each timestamp */
        for (i = 0; i < batch.count; i++) {
            fzcam_sync_ts_t *ts = &batch.timestamps[i];
            struct timespec ts_mono;

            /* Convert capture_timestamp_ns to timespec for human-readable display */
            ts_mono.tv_sec  = ts->capture_timestamp_ns / 1000000000ULL;
            ts_mono.tv_nsec = ts->capture_timestamp_ns % 1000000000ULL;

            printf("[FZCAM_SYNC] seq=%u gen=%u edge=%u "
                   "tsc=%llu capture=%lld.%09ld flags=0x%x\n",
                   ts->sequence,
                   ts->generator_id,
                   ts->edge_id,
                   (unsigned long long)ts->tsc_counter_ns,
                   (long long)ts_mono.tv_sec,
                   ts_mono.tv_nsec,
                   ts->flags);
            fflush(stdout);
        }
    }

    /* Graceful shutdown */
    fprintf(stderr, "fzcam_sync_daemon: stopping TSC...\n");

    ioctl(fd, FZCAM_SYNC_STOP);

    close(fd);

    fprintf(stderr, "fzcam_sync_daemon: stopped.\n");
    return 0;
}
