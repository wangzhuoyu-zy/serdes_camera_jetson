/*
 * isx031_flash.h — ISX031 内部 Flash 访问协议 (C 版本)
 *
 * 协议来源 (raw I2C, 无寄存器前缀):
 *   i2ctransfer -y -f $bus w3@$addr 0xFF 0xFF 0xF4    # 解锁序列 1
 *   i2ctransfer -y -f $bus w3@$addr 0xFF 0xFF 0xF7    # 解锁序列 2
 *   i2ctransfer -y -f $bus w8@$addr 0x80 0x00 CMD 0x00 ADDR[3] ADDR[2] ADDR[1] LEN
 *   i2ctransfer -y -f $bus w2@$addr 0x00 0x00 r$len    # 读数据端口
 *
 * Flash 访问约束 (来自《ISX031标定内参地址映射V1.1(3)》):
 *   1. 一次访问 (读/写) 必须以 256 Byte 为单位 (本工具用 chunked 方式自动按 256B 分片)
 *   2. Flash 擦除以 Sector 为单位, 一个 Sector = 4096 Byte
 *   3. 在 4 Mbit 区域内没有 Reserve Area, 给用户使用
 *      若要存 AA 中间结果或内参, 需选用 8 Mbit 或更大 Flash
 *   4. 目标地址已有数据时, 写入前必须先擦除该 Sector
 *
 * 目标平台: Jetson Orin (ARM64 Linux, Ubuntu 22.04)
 */

#ifndef ISX031_FLASH_H
#define ISX031_FLASH_H

/* 启用 strdup / strtok_r / usleep 等 GNU 扩展 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

/* ------------------------------------------------------------------ */
/* Flash 协议常量                                                      */
/* ------------------------------------------------------------------ */
#define ISX031_UNLOCK_PAYLOAD_1   "\xff\xff\xf4"   /* 3 字节 raw 写: 解锁序列 1 */
#define ISX031_UNLOCK_PAYLOAD_2   "\xff\xff\xf7"   /* 3 字节 raw 写: 解锁序列 2 */
#define ISX031_DATA_PORT_TRIGGER  "\x00\x00"       /* 2 字节 raw 写: 触发读数据端口 */
#define ISX031_CMD_PREFIX_HI      0x80             /* 命令包前缀 */
#define ISX031_CMD_PREFIX_LO      0x00

#define ISX031_CMD_FLASH_READ     0x01
#define ISX031_CMD_FLASH_WRITE    0x02
#define ISX031_CMD_FLASH_ERASE    0x03   /* 与 0x800003 等价的 raw 命令码 */

/* 命令包 byte[7] 的协议常量 (Sensor 固件内部 buffer size)
 *
 * ⚠️ 重要: byte[7] 不是要读/写的字节数, 而是 Sensor 固件期望的固定 magic
 *   用户实测: 必须填 0x5A, 否则 Sensor 返回全 0 (实际数据读不出来)
 *   该值与 i2ctransfer 命令中的 r$N / w$N 长度独立 —— 实际读/写字节数由
 *   后续的 r$N 或 w$N 决定
 *
 *   i2ctransfer -y -f 9 w8@0x3a 80 00 01 00 00 C2 00 5A   ← byte[7] 固定 0x5A
 *   i2ctransfer -y -f 9 w2@0x3a 00 00 r104                ← 实际读 104 字节 (整段内参)
 */
#define ISX031_CMD_PKT_LEN_MAGIC  0x5A             /* 命令包 byte[7] 固定值 = 90 */

/* Flash 物理约束 */
#define ISX031_CHUNK_SIZE         256    /* 单条 read/write 命令实际传输上限 (Sensor 实际支持到 104, 留余量) */
#define ISX031_SECTOR_SIZE        4096   /* 一个擦除 Sector 的大小 */
#define ISX031_MAX_PACKET_LEN     255    /* 命令包中长度字段为单字节 (≤255) */
#define ISX031_ERASE_POLL_MS      50     /* Sector 擦除等待轮询间隔 */
#define ISX031_ERASE_TIMEOUT_MS   5000   /* Sector 擦除超时 */

/* 全局调试开关: 1 = 打印所有 I2C 报文字节级细节 */
extern int isx031_debug_i2c;

/* 内参区起始地址 (24-bit Flash 地址, 与 Python 工具保持一致) */
#define ISX031_INTRINSIC_BASE_ADDR 0x00C200u

/* 默认解串器映射后的 Sensor I2C 地址 (7-bit) */
static const unsigned char ISX031_DEFAULT_DESER_ADDRS[] = { 0x3A, 0x3B, 0x3C, 0x3D };
#define ISX031_DEFAULT_DESER_ADDR_COUNT 4

/* ------------------------------------------------------------------ */
/* 元数据区 (software version / model ID / SN)                          */
/* ------------------------------------------------------------------ */
#define ISX031_SW_VERSION_ADDR   0x00C100u   /* 6 字节 ASCII, 例 "TMV1.1" */
#define ISX031_SW_VERSION_LEN    6
#define ISX031_MODEL_ID_ADDR     0x00C106u   /* 1 字节, 见 isx031_model_t */
#define ISX031_SN_ADDR           0x00C300u   /* 20 字节 ASCII, 例 "TMMX03CE022511130001" */
#define ISX031_SN_LEN            20

/* ------------------------------------------------------------------ */
/* Sensor 型号 (Model ID → 字段映射)                                     */
/* ------------------------------------------------------------------ */
/* Model ID 来自 0xC106 处的 1 字节 */
typedef enum {
    ISX031_MODEL_UNKNOWN = 0x00,  /* 探测失败或不支持 */
    ISX031_MODEL_H200    = 0x01,  /* Fisheye model, fx/fy/cx/cy + K1..K4 + repro */
    ISX031_MODEL_H69     = 0x02,  /* Pinhole model,  fx/fy/cx/cy + K1..K6 + P1 + P2 + repro */
    ISX031_MODEL_H120    = 0x03,  /* Pinhole model */
    ISX031_MODEL_H64     = 0x04,  /* Pinhole model */
    ISX031_MODEL_H100    = 0x05,  /* Pinhole model */
    ISX031_MODEL_H30     = 0x06,  /* Pinhole model */
} isx031_model_t;

/* 模型名称表 (供 CLI / 日志使用) */
extern const char *const ISX031_MODEL_NAMES[7];

/* 模型的"度数字段"标签 (H200=200°, H69=69°, ...) */
extern const char *const ISX031_MODEL_DEGREES[7];

/* 模型 → 畸变模型字符串 (Fisheye vs Pinhole) */
#define ISX031_MODEL_TYPE_FISHEYE "fisheye"
#define ISX031_MODEL_TYPE_PINHOLE "pinhole"
const char *isx031_model_type(isx031_model_t m);
const char *isx031_model_name(isx031_model_t m);

/* ------------------------------------------------------------------ */
/* 内参字段布局                                                         */
/* ------------------------------------------------------------------ */
/* H200 (Fisheye) 字段顺序 (9 字段 × 8 = 72 字节, 0xC200..0xC247):
 *   fx, fy, cx, cy, K1, K2, K3, K4, repro
 *
 * H120/H100/H69/H64/H30 (Pinhole) 字段顺序 (13 字段 × 8 = 104 字节, 0xC200..0xC267):
 *   fx, fy, cx, cy, K1..K6, P1, P2, repro
 *
 * 字段类型: IEEE-754 float64 (double), 小端 (little-endian), 每字段 8 字节
 */
typedef enum {
    ISX031_FIELD_FX = 0,
    ISX031_FIELD_FY,
    ISX031_FIELD_CX,
    ISX031_FIELD_CY,
    ISX031_FIELD_K1,
    ISX031_FIELD_K2,
    ISX031_FIELD_K3,
    ISX031_FIELD_K4,
    ISX031_FIELD_K5,    /* 仅 Pinhole */
    ISX031_FIELD_K6,    /* 仅 Pinhole */
    ISX031_FIELD_P1,    /* 仅 Pinhole */
    ISX031_FIELD_P2,    /* 仅 Pinhole */
    ISX031_FIELD_REPRO, /* 各模型最后一个 double 字段, README 标为 "reprojection error" */
    ISX031_FIELD__COUNT
} isx031_field_t;

#define ISX031_FIELD_SIZE   8

/* 每个型号对应的字段总数 + 字段地址表 (由实现填充) */
#define ISX031_MAX_FIELDS_PER_MODEL 13

typedef struct {
    int          n_fields;                                      /* 该模型的字段数 (8/13) */
    isx031_field_t fields[ISX031_MAX_FIELDS_PER_MODEL];         /* 字段顺序 */
    uint32_t     field_addr[ISX031_MAX_FIELDS_PER_MODEL];       /* 对应 Flash 24-bit 地址 */
    const char  *field_name[ISX031_MAX_FIELDS_PER_MODEL];      /* 字段名 (供 JSON/YAML 输出) */
} isx031_model_layout_t;

/* 查询指定模型的字段布局 (返回静态常量指针) */
const isx031_model_layout_t *isx031_get_layout(isx031_model_t m);

/* 内参数据结构 (按"最大可能"分配, 解析时按模型裁剪)
 *   H200 用: fx/fy/cx/cy + K1..K4 + repro
 *   Pinhole 用: fx/fy/cx/cy + K1..K6 + P1 + P2 + repro
 */
typedef struct {
    double fx, fy, cx, cy;
    double k1, k2, k3, k4, k5, k6;
    double p1, p2;
    double repro;
} isx031_intrinsics_t;

/* 元数据 (每次 read 时一并读取, 方便上层 JSON 输出) */
typedef struct {
    char           sw_version[ISX031_SW_VERSION_LEN + 1];  /* null-terminated */
    unsigned char  model_id;
    isx031_model_t model;
    char           sn[ISX031_SN_LEN + 1];                /* null-terminated */
} isx031_meta_t;

/* H200 (Fisheye) 字段数 + 总字节 (fx/fy/cx/cy + K1..K4 + repro = 9 字段) */
#define ISX031_FIELDS_H200        9
#define ISX031_BYTES_H200         (ISX031_FIELD_SIZE * ISX031_FIELDS_H200)  /* 72 */

/* Pinhole (H120/H100/H69/H64/H30) 字段数 + 总字节 */
#define ISX031_FIELDS_PINHOLE     13
#define ISX031_BYTES_PINHOLE      (ISX031_FIELD_SIZE * ISX031_FIELDS_PINHOLE) /* 104 */

/* 兼容旧 API: 默认按 Pinhole 计算, 与历史一致 */
#define ISX031_TOTAL_FIELDS       ISX031_FIELDS_PINHOLE
#define ISX031_TOTAL_BYTES        ISX031_BYTES_PINHOLE
#define ISX031_H200_BYTES         ISX031_BYTES_H200

/* 字段名常量字符串表 (Pinhole 全字段, 13 项) */
extern const char *const ISX031_FIELD_NAMES[ISX031_TOTAL_FIELDS];

/* Pinhole 字段起始地址 (与 isx031_intrinsics_map.py 一致) */
extern const uint32_t   ISX031_FIELD_ADDRS_PINHOLE[ISX031_TOTAL_FIELDS];
/* H200 字段起始地址 (10 项) */
extern const uint32_t   ISX031_FIELD_ADDRS_H200[ISX031_FIELDS_H200];

/* ------------------------------------------------------------------ */
/* I2C 底层 API                                                        */
/* ------------------------------------------------------------------ */

/* 打开 /dev/i2c-N, 返回 fd, 失败返回 -1 */
int isx031_i2c_open(const char *dev_path);

/* raw 写: 单条 i2c_msg.write, payload 不含寄存器地址
 *   等价于 i2ctransfer -y -f $bus wN@$addr payload[0..N-1]
 *  返回 0 成功, -1 失败
 */
int isx031_i2c_raw_write(int fd, unsigned char addr,
                         const void *payload, unsigned int len);

/* raw 写 + 读 (一次 i2c_rdwr):
 *   等价于 i2ctransfer -y -f $bus wM@$addr wpayload[0..M-1] rN
 *   返回读取的字节数, 失败返回 -1
 */
int isx031_i2c_raw_write_then_read(int fd, unsigned char addr,
                                   const void *wpayload, unsigned int wlen,
                                   void *rbuf, unsigned int rlen);

/* I2C 总线扫描, 把 0x03..0x77 范围内有 ACK 的地址填入 out (容量 out_cap),
 *   返回发现的设备数量, 失败返回 -1
 */
int isx031_i2c_scan(int fd, unsigned char *out, int out_cap);

/* ------------------------------------------------------------------ */
/* Flash 协议 API (直接对应 sony.cpp / isx031_flash.py)                */
/* ------------------------------------------------------------------ */

/* Flash 解锁 (执行两阶段解锁序列) */
int isx031_flash_unlock(int fd, unsigned char addr);

/* 构造命令包: [0x80, 0x00, cmd, 0x00, addr_b2, addr_b1, addr_b0, len]
 *   addr_b2..addr_b0 = 24-bit Flash 地址大端
 *   len = 单字节长度 (1..255)
 */
int isx031_flash_send_cmd(int fd, unsigned char addr,
                          unsigned char cmd, uint32_t addr24,
                          unsigned int length);

/* 读取 N 字节 (N <= ISX031_CHUNK_SIZE=256).
 *   协议: unlock → cmd(READ,addr,len) → write_then_read([0x00,0x00], len)
 *   返回读取字节数, 失败返回 -1
 */
int isx031_flash_read_chunk(int fd, unsigned char addr,
                            uint32_t addr24,
                            unsigned char *buf, unsigned int len);

/* 写入 N 字节 (N <= ISX031_CHUNK_SIZE=256).
 *   协议: unlock → cmd(WRITE,addr,len) → raw_write(data, len)
 *   ⚠️ 调用方必须保证目标 Sector 已擦除
 *   返回写入字节数, 失败返回 -1
 */
int isx031_flash_write_chunk(int fd, unsigned char addr,
                             uint32_t addr24,
                             const unsigned char *buf, unsigned int len);

/* 擦除一个 Sector (4096 Byte), 从 addr24 所在的 Sector 起始地址开始 */
int isx031_flash_sector_erase(int fd, unsigned char addr, uint32_t addr24);

/* 轮询 Sector 擦除完成 (读 status 寄存器); 阻塞至完成或超时 */
int isx031_flash_wait_erase_done(int fd, unsigned char addr,
                                 unsigned int timeout_ms);

/* 读 Flash 任意长度 (内部自动按 90B 分片) */
int isx031_flash_read(int fd, unsigned char addr,
                      uint32_t addr24, unsigned int length,
                      unsigned char *buf);

/* 写 Flash 任意长度 (内部自动按 90B 分片 + 自动擦除涉及的 Sector) */
int isx031_flash_write(int fd, unsigned char addr,
                       uint32_t addr24, unsigned int length,
                       const unsigned char *buf);

/* 单条命令 packet 中的 byte[7] 必须填 0x5A (协议 magic), 不是实际长度
 * 因此 flash_read / flash_write 在内部按 90B 自动分片调用 flash_read_chunk / _write_chunk
 */

/* ------------------------------------------------------------------ */
/* 内参 + 元数据 API                                                    */
/* ------------------------------------------------------------------ */

/* Model ID 探测: 读 0xC106 的 1 字节, 返回对应枚举; 失败返回 ISX031_MODEL_UNKNOWN */
isx031_model_t isx031_detect_model(int fd, unsigned char addr);

/* 元数据读取 (sw_version / model_id / SN); 失败返回 -1 */
int isx031_read_meta(int fd, unsigned char addr, isx031_meta_t *out);

/* 把单个 (字段, 值) 打包为 8 字节小端 float64 */
void isx031_pack_field(isx031_field_t f, double v, unsigned char out[8]);

/* 从 8 字节小端数据解析为 double */
double isx031_unpack_field(const unsigned char in[8]);

/* 按指定模型布局解析 raw 字节为 isx031_intrinsics_t
 *   raw_len 必须 >= 该模型字段数 * 8
 */
void isx031_parse_intrinsics(const unsigned char *raw,
                             unsigned int raw_len,
                             isx031_model_t m,
                             isx031_intrinsics_t *out);

/* 读取某地址的全部内参 (按指定模型读取相应字节数), 失败返回 -1
 *   raw_out 容量需 >= 该模型总字节数 (H200=80, Pinhole=104)
 */
int isx031_read_intrinsics(int fd, unsigned char addr, isx031_model_t m,
                           isx031_intrinsics_t *out,
                           unsigned char *raw_out, unsigned int raw_cap);

/* 一站式: 探测 model + 读 meta + 读 intrinsics
 *   out_model 不可为 NULL; meta/intr 可为 NULL 表示不读
 */
int isx031_read_all(int fd, unsigned char addr,
                    isx031_model_t *out_model,
                    isx031_meta_t *meta,         /* 可为 NULL */
                    isx031_intrinsics_t *intr);  /* 可为 NULL */

/* 写入某字段 (单字段, 自动擦除所在 Sector) */
int isx031_write_field(int fd, unsigned char addr,
                       isx031_field_t f, double v);

/* 写入全部内参 (自动擦除涉及的 Sector, 按指定模型写入) */
int isx031_write_intrinsics(int fd, unsigned char addr, isx031_model_t m,
                            const isx031_intrinsics_t *v);

/* ------------------------------------------------------------------ */
/* 实用工具                                                            */
/* ------------------------------------------------------------------ */

/* 7-bit I2C 地址 ↔ 8-bit 转换 */
static inline unsigned char isx031_addr7_to_addr8(unsigned char a7) { return (unsigned char)((a7 << 1) & 0xFF); }
static inline unsigned char isx031_addr8_to_addr7(unsigned char a8) { return (unsigned char)((a8 >> 1) & 0x7F); }

/* CRC16-CCITT (XMODEM, poly=0x1021, init=0xFFFF) */
unsigned short isx031_crc16_ccitt(const unsigned char *data, unsigned int len,
                                  unsigned short seed);

/* 错误日志 */
#define ISX031_ERR(fmt, ...) \
    fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#define ISX031_WARN(fmt, ...) \
    fprintf(stderr, "[WARN]  " fmt "\n", ##__VA_ARGS__)
#define ISX031_INFO(fmt, ...) \
    fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__)

#endif /* ISX031_FLASH_H */