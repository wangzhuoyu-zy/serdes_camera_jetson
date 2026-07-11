/*
 * isx031_flash.c — ISX031 内部 Flash 访问协议 (C 实现)
 *
 * 设计要点:
 *   - 直接对应 Python isx031_flash.py 的 raw I2C 协议 (用户实测 i2ctransfer 序列)
 *   - 解锁序列 / 命令包 / 数据端口触发, 全部用 raw I2C 报文, 不走 16-bit 寄存器
 *   - 自动处理 256B chunked 读写, 自动按 Sector (4096B) 预擦除
 *   - 复用 sensor_flash_cfg.c 的 ioctl(I2C_RDWR) 风格
 *
 * 编译: see Makefile
 */

#include "isx031_flash.h"

/* 全局调试开关 (由 CLI --verbose 控制) */
int isx031_debug_i2c = 0;

/* 打印十六进制 dump 到 stderr */
static void dbg_dump(const char *tag, const unsigned char *buf, unsigned int len)
{
    if (!isx031_debug_i2c) return;
    fprintf(stderr, "  [DBG] %s (%u):", tag, len);
    for (unsigned int i = 0; i < len; i++) fprintf(stderr, " %02X", buf[i]);
    fprintf(stderr, "\n");
}

/* ------------------------------------------------------------------ */
/* 全局表                                                              */
/* ------------------------------------------------------------------ */

/* Model 名表 (按 enum 值索引, 0 = UNKNOWN) */
const char *const ISX031_MODEL_NAMES[7] = {
    [ISX031_MODEL_UNKNOWN] = "UNKNOWN",
    [ISX031_MODEL_H200]    = "H200",
    [ISX031_MODEL_H69]     = "H69",
    [ISX031_MODEL_H120]    = "H120",
    [ISX031_MODEL_H64]     = "H64",
    [ISX031_MODEL_H100]    = "H100",
    [ISX031_MODEL_H30]     = "H30",
};

const char *const ISX031_MODEL_DEGREES[7] = {
    [ISX031_MODEL_UNKNOWN] = "?",
    [ISX031_MODEL_H200]    = "200°",
    [ISX031_MODEL_H69]     = "69°",
    [ISX031_MODEL_H120]    = "120°",
    [ISX031_MODEL_H64]     = "64°",
    [ISX031_MODEL_H100]    = "100°",
    [ISX031_MODEL_H30]     = "30°",
};

const char *isx031_model_name(isx031_model_t m)
{
    int idx = (int)m;
    if (idx < 0 || idx >= 7) return "INVALID";
    return ISX031_MODEL_NAMES[idx];
}

const char *isx031_model_type(isx031_model_t m)
{
    return (m == ISX031_MODEL_H200) ? ISX031_MODEL_TYPE_FISHEYE
                                    : ISX031_MODEL_TYPE_PINHOLE;
}

/* Pinhole (H69/H120/H64/H100/H30) 字段顺序 + 地址 + 名字 (字段顺序在 LAYOUT_PINHOLE 中) */

const char *const ISX031_FIELD_NAMES[ISX031_TOTAL_FIELDS] = {
    [ISX031_FIELD_FX]     = "fx",
    [ISX031_FIELD_FY]     = "fy",
    [ISX031_FIELD_CX]     = "cx",
    [ISX031_FIELD_CY]     = "cy",
    [ISX031_FIELD_K1]     = "k1",
    [ISX031_FIELD_K2]     = "k2",
    [ISX031_FIELD_K3]     = "k3",
    [ISX031_FIELD_K4]     = "k4",
    [ISX031_FIELD_K5]     = "k5",
    [ISX031_FIELD_K6]     = "k6",
    [ISX031_FIELD_P1]     = "p1",
    [ISX031_FIELD_P2]     = "p2",
    [ISX031_FIELD_REPRO]  = "repro",
};

const uint32_t ISX031_FIELD_ADDRS_PINHOLE[ISX031_TOTAL_FIELDS] = {
    [ISX031_FIELD_FX]     = 0x00C200,
    [ISX031_FIELD_FY]     = 0x00C208,
    [ISX031_FIELD_CX]     = 0x00C210,
    [ISX031_FIELD_CY]     = 0x00C218,
    [ISX031_FIELD_K1]     = 0x00C220,
    [ISX031_FIELD_K2]     = 0x00C228,
    [ISX031_FIELD_K3]     = 0x00C230,
    [ISX031_FIELD_K4]     = 0x00C238,
    [ISX031_FIELD_K5]     = 0x00C240,
    [ISX031_FIELD_K6]     = 0x00C248,
    [ISX031_FIELD_P1]     = 0x00C250,
    [ISX031_FIELD_P2]     = 0x00C258,
    [ISX031_FIELD_REPRO]  = 0x00C260,
};

/* H200 (Fisheye) 字段起始地址表 (9 字段, 与 README H200 表完全一致) */
const uint32_t ISX031_FIELD_ADDRS_H200[ISX031_FIELDS_H200] = {
    0x00C200,  /* fx    */
    0x00C208,  /* fy    */
    0x00C210,  /* cx    */
    0x00C218,  /* cy    */
    0x00C220,  /* k1    */
    0x00C228,  /* k2    */
    0x00C230,  /* k3    */
    0x00C238,  /* k4    */
    0x00C240,  /* repro */
};

/* 各模型的完整字段布局 (供 JSON/YAML 输出用) */
static const isx031_model_layout_t LAYOUT_H200 = {
    .n_fields = ISX031_FIELDS_H200,
    .fields   = { ISX031_FIELD_FX, ISX031_FIELD_FY, ISX031_FIELD_CX, ISX031_FIELD_CY,
                  ISX031_FIELD_K1, ISX031_FIELD_K2, ISX031_FIELD_K3, ISX031_FIELD_K4,
                  ISX031_FIELD_REPRO },
    .field_addr = { 0x00C200, 0x00C208, 0x00C210, 0x00C218,
                    0x00C220, 0x00C228, 0x00C230, 0x00C238, 0x00C240 },
    .field_name = { "fx", "fy", "cx", "cy",
                    "k1", "k2", "k3", "k4",
                    "repro" },
};

static const isx031_model_layout_t LAYOUT_PINHOLE = {
    .n_fields = ISX031_FIELDS_PINHOLE,
    .fields   = { ISX031_FIELD_FX, ISX031_FIELD_FY, ISX031_FIELD_CX, ISX031_FIELD_CY,
                  ISX031_FIELD_K1, ISX031_FIELD_K2, ISX031_FIELD_K3, ISX031_FIELD_K4,
                  ISX031_FIELD_K5, ISX031_FIELD_K6,
                  ISX031_FIELD_P1, ISX031_FIELD_P2,
                  ISX031_FIELD_REPRO },
    .field_addr = { 0x00C200, 0x00C208, 0x00C210, 0x00C218,
                    0x00C220, 0x00C228, 0x00C230, 0x00C238,
                    0x00C240, 0x00C248,
                    0x00C250, 0x00C258,
                    0x00C260 },
    .field_name = { "fx", "fy", "cx", "cy",
                    "k1", "k2", "k3", "k4",
                    "k5", "k6",
                    "p1", "p2",
                    "repro" },
};

const isx031_model_layout_t *isx031_get_layout(isx031_model_t m)
{
    switch (m) {
        case ISX031_MODEL_H200:    return &LAYOUT_H200;
        case ISX031_MODEL_H69:
        case ISX031_MODEL_H120:
        case ISX031_MODEL_H64:
        case ISX031_MODEL_H100:
        case ISX031_MODEL_H30:     return &LAYOUT_PINHOLE;
        default:                   return NULL;
    }
}

/* ------------------------------------------------------------------ */
/* I2C 底层操作                                                        */
/* ------------------------------------------------------------------ */

int isx031_i2c_open(const char *dev_path)
{
    int fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        ISX031_ERR("open %s: %s", dev_path, strerror(errno));
        return -1;
    }
    return fd;
}

int isx031_i2c_raw_write(int fd, unsigned char addr,
                         const void *payload, unsigned int len)
{
    struct i2c_msg msg;
    struct i2c_rdwr_ioctl_data io;

    if (len == 0)
        return 0;
    if (len > 8192) {
        ISX031_ERR("raw_write: too long (%u)", len);
        return -1;
    }

    msg.addr  = (unsigned short)addr;
    msg.flags = 0;
    msg.len   = (unsigned short)len;
    msg.buf   = (unsigned char *)payload;

    io.msgs  = &msg;
    io.nmsgs = 1;

    dbg_dump("WR", payload, len);
    if (ioctl(fd, I2C_RDWR, &io) < 0) {
        int e = errno;
        ISX031_ERR("raw_write FAIL: addr=0x%02x len=%u (%s)",
                   addr, len, strerror(e));
        /* 常见原因提示 */
        if (e == ENXIO)
            fprintf(stderr, "         hint: Sensor 未应答数据字节. 可能原因:\n"
                            "               - 解串器/SerDes 未初始化, 链路未通\n"
                            "               - Sensor 处于低功耗 / standby 状态\n"
                            "               - 解串器未把 0x3A 映射到该 Sensor\n"
                            "               - 内核驱动占用 /dev/i2c-9, 需先 unbind\n");
        else if (e == EBUSY)
            fprintf(stderr, "         hint: 总线忙, 上一笔事务未完成, 请稍后重试\n");
        else if (e == EAGAIN)
            fprintf(stderr, "         hint: 总线超时, 检查硬件连接\n");
        return -1;
    }
    return 0;
}

int isx031_i2c_raw_write_then_read(int fd, unsigned char addr,
                                   const void *wpayload, unsigned int wlen,
                                   void *rbuf, unsigned int rlen)
{
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data io;

    if (wlen == 0 || rlen == 0)
        return -1;

    msgs[0].addr  = (unsigned short)addr;
    msgs[0].flags = 0;
    msgs[0].len   = (unsigned short)wlen;
    msgs[0].buf   = (unsigned char *)wpayload;

    msgs[1].addr  = (unsigned short)addr;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len   = (unsigned short)rlen;
    msgs[1].buf   = (unsigned char *)rbuf;

    io.msgs  = msgs;
    io.nmsgs = 2;

    dbg_dump("WR", wpayload, wlen);
    if (ioctl(fd, I2C_RDWR, &io) < 0) {
        ISX031_ERR("raw_write_then_read FAIL: addr=0x%02x w=%u r=%u (%s)",
                   addr, wlen, rlen, strerror(errno));
        return -1;
    }
    dbg_dump("RD", rbuf, rlen);
    return (int)rlen;
}

int isx031_i2c_scan(int fd, unsigned char *out, int out_cap)
{
    int found = 0;
    /* 探测策略: 1 字节读, 等价 i2cdetect -y 默认行为
     *   - 每次先 ioctl(I2C_SLAVE, a)
     *   - 然后 I2C_RDWR 读 1 字节, 设备 ACK 则视为存在
     *   - 该方式不会像 raw_write 那样触发设备 NACK
     * 与 send-only 的零长度 rdwr 不同, 这里使用 read-then-check 模式,
     *   兼容 Tegra I2C 控制器 (在 zero-length 模式下可能直接返回 0 不验证 ACK)
     */
    for (unsigned char a = 0x03; a <= 0x77; a++) {
        if (found >= out_cap)
            break;
        if (ioctl(fd, I2C_SLAVE, (int)a) < 0)
            continue;
        unsigned char b = 0;
        struct i2c_msg msgs[2] = {
            { .addr = a, .flags = 0,       .len = 0, .buf = NULL },  /* dummy write phase */
            { .addr = a, .flags = I2C_M_RD, .len = 1, .buf = &b  },
        };
        struct i2c_rdwr_ioctl_data io = { .msgs = msgs, .nmsgs = 2 };
        if (ioctl(fd, I2C_RDWR, &io) >= 0)
            out[found++] = a;
    }
    return found;
}

/* ------------------------------------------------------------------ */
/* Flash 协议实现                                                      */
/* ------------------------------------------------------------------ */

int isx031_flash_unlock(int fd, unsigned char addr)
{
    if (isx031_i2c_raw_write(fd, addr,
                             ISX031_UNLOCK_PAYLOAD_1,
                             (unsigned int)strlen(ISX031_UNLOCK_PAYLOAD_1)) < 0)
        return -1;
    usleep(2000);
    if (isx031_i2c_raw_write(fd, addr,
                             ISX031_UNLOCK_PAYLOAD_2,
                             (unsigned int)strlen(ISX031_UNLOCK_PAYLOAD_2)) < 0)
        return -1;
    usleep(2000);
    return 0;
}

int isx031_flash_send_cmd(int fd, unsigned char addr,
                          unsigned char cmd, uint32_t addr24,
                          unsigned int length)
{
    /* ⚠️ 命令包 byte[7] 不是实际要读/写的字节数, 而是 Sensor 固件内部 buffer 的
     * 固定 magic (0x5A = 90). 用户实测: 填别的值 Sensor 会返回全 0
     * length 参数保留仅供上层日志/校验, 不会写进 byte[7]
     */
    (void)length;
    unsigned char pkt[8] = {
        ISX031_CMD_PREFIX_HI,
        ISX031_CMD_PREFIX_LO,
        cmd & 0xFF,
        0x00,
        (unsigned char)((addr24 >> 16) & 0xFF),  /* addr_b2 (MSB) */
        (unsigned char)((addr24 >> 8)  & 0xFF),  /* addr_b1 */
        (unsigned char)(addr24         & 0xFF),  /* addr_b0 (LSB) */
        ISX031_CMD_PKT_LEN_MAGIC,                /* byte[7] 固定 0x5A */
    };
    return isx031_i2c_raw_write(fd, addr, pkt, sizeof(pkt));
}

int isx031_flash_read_chunk(int fd, unsigned char addr,
                            uint32_t addr24,
                            unsigned char *buf, unsigned int len)
{
    if (len == 0 || len > ISX031_CHUNK_SIZE)
        return -1;

    if (isx031_flash_unlock(fd, addr) < 0)
        return -1;
    if (isx031_flash_send_cmd(fd, addr, ISX031_CMD_FLASH_READ, addr24, len) < 0)
        return -1;
    usleep(10000);

    int n = isx031_i2c_raw_write_then_read(fd, addr,
                                           ISX031_DATA_PORT_TRIGGER,
                                           sizeof(ISX031_DATA_PORT_TRIGGER) - 1,  /* wlen = 2, 不能用 strlen (含 NUL) */
                                           buf, len);
    if (n < 0)
        return -1;
    usleep(10000);
    return n;
}

int isx031_flash_write_chunk(int fd, unsigned char addr,
                             uint32_t addr24,
                             const unsigned char *buf, unsigned int len)
{
    if (len == 0 || len > ISX031_CHUNK_SIZE)
        return -1;

    if (isx031_flash_unlock(fd, addr) < 0)
        return -1;
    if (isx031_flash_send_cmd(fd, addr, ISX031_CMD_FLASH_WRITE, addr24, len) < 0)
        return -1;
    usleep(10000);
    if (isx031_i2c_raw_write(fd, addr, buf, len) < 0)
        return -1;
    usleep(10000);
    return (int)len;
}

int isx031_flash_sector_erase(int fd, unsigned char addr, uint32_t addr24)
{
    if (isx031_flash_unlock(fd, addr) < 0)
        return -1;
    if (isx031_flash_send_cmd(fd, addr, ISX031_CMD_FLASH_ERASE, addr24, 0) < 0)
        return -1;
    usleep(10000);
    return 0;
}

int isx031_flash_wait_erase_done(int fd, unsigned char addr,
                                 unsigned int timeout_ms)
{
    /* 简化策略: 固定 sleep, 由调用方根据设备实测调整 */
    /* fd / addr 预留给后续读取 status 寄存器的实现 */
    (void)fd;
    (void)addr;
    unsigned int slept = 0;
    const unsigned int step = ISX031_ERASE_POLL_MS;
    const unsigned int total = (timeout_ms == 0) ? ISX031_ERASE_TIMEOUT_MS : timeout_ms;
    while (slept < total) {
        usleep(step * 1000);
        slept += step;
    }
    return 0;
}

int isx031_flash_read(int fd, unsigned char addr,
                      uint32_t addr24, unsigned int length,
                      unsigned char *buf)
{
    uint32_t cur = addr24;
    unsigned int remain = length;
    unsigned char *p = buf;
    while (remain > 0) {
        unsigned int chunk = (remain > ISX031_CHUNK_SIZE) ? ISX031_CHUNK_SIZE : remain;
        int n = isx031_flash_read_chunk(fd, addr, cur, p, chunk);
        if (n < 0)
            return -1;
        p      += chunk;
        cur    += chunk;
        remain -= chunk;
    }
    return (int)length;
}

/* 计算覆盖 [start, start+len) 所需的全部 Sector 起始地址 (去重, 升序) */
static int collect_sectors(uint32_t start, unsigned int len,
                           uint32_t *out, int out_cap)
{
    uint32_t first_sector = start & ~((uint32_t)ISX031_SECTOR_SIZE - 1);
    uint32_t last_byte    = start + len - 1;
    uint32_t last_sector  = last_byte & ~((uint32_t)ISX031_SECTOR_SIZE - 1);
    int n = 0;
    for (uint32_t s = first_sector; s <= last_sector; s += ISX031_SECTOR_SIZE) {
        if (n >= out_cap)
            return -1;
        out[n++] = s;
    }
    return n;
}

int isx031_flash_write(int fd, unsigned char addr,
                       uint32_t addr24, unsigned int length,
                       const unsigned char *buf)
{
    if (length == 0)
        return 0;
    if (length > 16 * ISX031_SECTOR_SIZE) {
        ISX031_ERR("write length %u too large (max %d)",
                   length, 16 * ISX031_SECTOR_SIZE);
        return -1;
    }

    /* 预擦除所有涉及的 Sector (约束 4: 写入前必须先擦除) */
    uint32_t sectors[16];
    int n_sec = collect_sectors(addr24, length, sectors, 16);
    if (n_sec < 0) {
        ISX031_ERR("write spans too many sectors");
        return -1;
    }
    for (int i = 0; i < n_sec; i++) {
        ISX031_INFO("erasing sector at 0x%05X ...", sectors[i]);
        if (isx031_flash_sector_erase(fd, addr, sectors[i]) < 0)
            return -1;
    }
    isx031_flash_wait_erase_done(fd, addr, 0);

    /* 分片写入 (约束 1: 每片 ≤ 256B) */
    uint32_t cur = addr24;
    unsigned int remain = length;
    const unsigned char *p = buf;
    while (remain > 0) {
        unsigned int chunk = (remain > ISX031_CHUNK_SIZE) ? ISX031_CHUNK_SIZE : remain;
        int n = isx031_flash_write_chunk(fd, addr, cur, p, chunk);
        if (n < 0)
            return -1;
        p      += chunk;
        cur    += chunk;
        remain -= chunk;
    }
    return (int)length;
}

/* ------------------------------------------------------------------ */
/* 内参打包/解包                                                       */
/* ------------------------------------------------------------------ */

void isx031_pack_field(isx031_field_t f, double v, unsigned char out[8])
{
    /* IEEE-754 double little-endian
     * 参数 f 预留给后续按字段做范围/类型校验 */
    (void)f;
    union { double d; unsigned char b[8]; } u;
    u.d = v;
    memcpy(out, u.b, 8);
}

double isx031_unpack_field(const unsigned char in[8])
{
    union { double d; unsigned char b[8]; } u;
    memcpy(u.b, in, 8);
    return u.d;
}

/* 把 struct 内的 double 指针按 enum 顺序展开 (覆盖所有可能字段) */
static double *field_ptr(isx031_intrinsics_t *v, isx031_field_t f)
{
    switch (f) {
        case ISX031_FIELD_FX:    return &v->fx;
        case ISX031_FIELD_FY:    return &v->fy;
        case ISX031_FIELD_CX:    return &v->cx;
        case ISX031_FIELD_CY:    return &v->cy;
        case ISX031_FIELD_K1:    return &v->k1;
        case ISX031_FIELD_K2:    return &v->k2;
        case ISX031_FIELD_K3:    return &v->k3;
        case ISX031_FIELD_K4:    return &v->k4;
        case ISX031_FIELD_K5:    return &v->k5;
        case ISX031_FIELD_K6:    return &v->k6;
        case ISX031_FIELD_P1:    return &v->p1;
        case ISX031_FIELD_P2:    return &v->p2;
        case ISX031_FIELD_REPRO: return &v->repro;
        default:                 return NULL;
    }
}

void isx031_parse_intrinsics(const unsigned char *raw,
                             unsigned int raw_len,
                             isx031_model_t m,
                             isx031_intrinsics_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!raw) return;

    const isx031_model_layout_t *L = isx031_get_layout(m);
    if (!L) return;  /* UNKNOWN 模型: 不解析 */

    for (int i = 0; i < L->n_fields; i++) {
        unsigned int off = (unsigned int)i * ISX031_FIELD_SIZE;
        if (off + ISX031_FIELD_SIZE > raw_len)
            break;
        double *p = field_ptr(out, L->fields[i]);
        if (!p) continue;
        *p = isx031_unpack_field(raw + off);
    }
}

isx031_model_t isx031_detect_model(int fd, unsigned char addr)
{
    unsigned char b = 0;
    int n = isx031_flash_read(fd, addr,
                              ISX031_MODEL_ID_ADDR,
                              1, &b);
    if (n < 0) {
        ISX031_ERR("detect_model: read 0xC106 failed");
        return ISX031_MODEL_UNKNOWN;
    }
    isx031_model_t m = (isx031_model_t)b;
    if (m < ISX031_MODEL_H200 || m > ISX031_MODEL_H30)
        return ISX031_MODEL_UNKNOWN;
    return m;
}

int isx031_read_meta(int fd, unsigned char addr, isx031_meta_t *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));

    unsigned char raw[32];

    /* sw_version 6B @ 0xC100 + model_id 1B @ 0xC106 → 一次性读 7 字节 */
    int n = isx031_flash_read(fd, addr,
                              ISX031_SW_VERSION_ADDR,
                              ISX031_SW_VERSION_LEN + 1,
                              raw);
    if (n < 0) {
        ISX031_ERR("read_meta: sw_version/model_id read failed");
        return -1;
    }
    memcpy(out->sw_version, raw, ISX031_SW_VERSION_LEN);
    out->sw_version[ISX031_SW_VERSION_LEN] = '\0';
    /* 去掉尾部非可打印字符 */
    for (int i = ISX031_SW_VERSION_LEN - 1; i >= 0; i--) {
        if ((unsigned char)out->sw_version[i] >= 0x20 &&
            (unsigned char)out->sw_version[i] <= 0x7E)
            break;
        out->sw_version[i] = '\0';
    }
    out->model_id = raw[ISX031_SW_VERSION_LEN];
    out->model = isx031_detect_model(fd, addr);

    /* SN 20B @ 0xC300 */
    n = isx031_flash_read(fd, addr,
                           ISX031_SN_ADDR,
                           ISX031_SN_LEN, raw);
    if (n < 0) {
        ISX031_WARN("read_meta: SN read failed (继续)");
    } else {
        memcpy(out->sn, raw, ISX031_SN_LEN);
        out->sn[ISX031_SN_LEN] = '\0';
        for (int i = ISX031_SN_LEN - 1; i >= 0; i--) {
            if ((unsigned char)out->sn[i] >= 0x20 &&
                (unsigned char)out->sn[i] <= 0x7E)
                break;
            out->sn[i] = '\0';
        }
    }
    return 0;
}

int isx031_read_intrinsics(int fd, unsigned char addr, isx031_model_t m,
                           isx031_intrinsics_t *out,
                           unsigned char *raw_out, unsigned int raw_cap)
{
    const isx031_model_layout_t *L = isx031_get_layout(m);
    if (!L) {
        ISX031_ERR("read_intrinsics: unknown model");
        return -1;
    }
    unsigned int need = (unsigned int)L->n_fields * ISX031_FIELD_SIZE;
    if (raw_cap < need) {
        ISX031_ERR("read_intrinsics: raw_cap=%u < need=%u", raw_cap, need);
        return -1;
    }
    int n = isx031_flash_read(fd, addr,
                              ISX031_INTRINSIC_BASE_ADDR,
                              need, raw_out);
    if (n < 0) return -1;
    isx031_parse_intrinsics(raw_out, need, m, out);
    return 0;
}

int isx031_read_all(int fd, unsigned char addr,
                    isx031_model_t *out_model,
                    isx031_meta_t *meta,
                    isx031_intrinsics_t *intr)
{
    if (!out_model) return -1;
    isx031_model_t m = isx031_detect_model(fd, addr);
    if (m == ISX031_MODEL_UNKNOWN) {
        ISX031_WARN("read_all: model detect failed, fallback to H120");
        m = ISX031_MODEL_H120;
    }
    *out_model = m;
    if (meta) {
        if (isx031_read_meta(fd, addr, meta) < 0) {
            ISX031_WARN("read_all: meta partial fail");
        }
    }
    if (intr) {
        const isx031_model_layout_t *L = isx031_get_layout(m);
        unsigned int cap = (unsigned int)L->n_fields * ISX031_FIELD_SIZE;
        unsigned char *buf = (unsigned char *)malloc(cap);
        if (!buf) return -1;
        int rc = isx031_read_intrinsics(fd, addr, m, intr, buf, cap);
        free(buf);
        if (rc < 0) return -1;
    }
    return 0;
}

int isx031_write_field(int fd, unsigned char addr,
                       isx031_field_t f, double v)
{
    if ((unsigned int)f >= ISX031_FIELD__COUNT)
        return -1;
    /* 默认按 Pinhole 地址写入 (兼容性最好) */
    uint32_t reg = ISX031_FIELD_ADDRS_PINHOLE[f];
    unsigned char buf[8];
    isx031_pack_field(f, v, buf);
    return isx031_flash_write(fd, addr, reg, ISX031_FIELD_SIZE, buf);
}

int isx031_write_intrinsics(int fd, unsigned char addr, isx031_model_t m,
                            const isx031_intrinsics_t *v)
{
    if (!v) return -1;
    const isx031_model_layout_t *L = isx031_get_layout(m);
    if (!L) {
        ISX031_ERR("write_intrinsics: unknown model");
        return -1;
    }
    unsigned int n_bytes = (unsigned int)L->n_fields * ISX031_FIELD_SIZE;
    unsigned char *buf = (unsigned char *)malloc(n_bytes);
    if (!buf) return -1;

    /* 按模型字段顺序把 v 打包到 buf */
    for (int i = 0; i < L->n_fields; i++) {
        const double *p = field_ptr((isx031_intrinsics_t *)v, L->fields[i]);
        if (!p) {
            memset(buf + i * ISX031_FIELD_SIZE, 0, ISX031_FIELD_SIZE);
            continue;
        }
        isx031_pack_field(L->fields[i], *p, buf + i * ISX031_FIELD_SIZE);
    }

    int rc = isx031_flash_write(fd, addr,
                                ISX031_INTRINSIC_BASE_ADDR,
                                n_bytes, buf);
    free(buf);
    return rc;
}

/* ------------------------------------------------------------------ */
/* 实用工具                                                            */
/* ------------------------------------------------------------------ */

unsigned short isx031_crc16_ccitt(const unsigned char *data, unsigned int len,
                                  unsigned short seed)
{
    unsigned short crc = seed;
    for (unsigned int i = 0; i < len; i++) {
        crc ^= (unsigned short)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            if (crc & 0x8000)
                crc = (unsigned short)((crc << 1) ^ 0x1021);
            else
                crc = (unsigned short)(crc << 1);
        }
    }
    return crc;
}