# 竹影智能Camera ISX031 内参读取

通过 Linux I2C (`/dev/i2c-N`) 读取与写入 ISX031 Sensor **内部 Flash** 中存放的相机内参的 C 工具。
支持所有 6 款 Sensor 型号 (H200/H120/H100/H69/H64/H30),自动按 Model ID 选择字段布局。

## 功能

- 通过 raw I2C 协议访问 ISX031 内部 Flash (无需 16-bit 寄存器寻址)
- 单字段或整段读取相机内参 (**按 Sensor 型号自动适配字段数量**)
- 单字段或整段写入 (写入前自动按 Sector 预擦除)
- 读取元数据: software version / model ID / SN
- **自动检测 Sensor 型号** (读 0xC106 的 Model ID),无需手动指定
- 导出 Flash raw 字节到文件 (`dump`)
- 扫描 I2C 总线 (`scan`)
- 输出 JSON / YAML / OpenCV-YAML 三种格式 (畸变系数维度按模型自适应)
- 单条命令支持多 Sensor 批量操作 (解串器映射地址 0x3A..0x3D)

## Sensor 型号与字段映射

### Model ID 对照表

| Model ID | 型号   | 畸变模型    | 视野   | 字段数 | 内参总字节      |
| -------- | ---- | ------- | ---- | --- | ---------- |
| 0x01     | H200 | Fisheye | 200° | 9   | 72  (0x48) |
| 0x02     | H69  | Pinhole | 69°  | 13  | 104 (0x68) |
| 0x03     | H120 | Pinhole | 120° | 13  | 104 (0x68) |
| 0x04     | H64  | Pinhole | 64°  | 13  | 104 (0x68) |
| 0x05     | H100 | Pinhole | 100° | 13  | 104 (0x68) |
| 0x06     | H30  | Pinhole | 30°  | 13  | 104 (0x68) |

### H200 (Fisheye) — 9 字段

| 地址     | 字段                 | 类型 / 字节    | 备注                        |
| ------ | ------------------ | ---------- | ------------------------- |
| 0xC100 | software version   | ASCII × 6  | 例: `TMV1.1`               |
| 0xC106 | Model ID           | hex × 1    | 0x01 = H200               |
| 0xC200 | fx                 | float64 LE | <br />                    |
| 0xC208 | fy                 | float64 LE | <br />                    |
| 0xC210 | cx                 | float64 LE | <br />                    |
| 0xC218 | cy                 | float64 LE | <br />                    |
| 0xC220 | k1                 | float64 LE | <br />                    |
| 0xC228 | k2                 | float64 LE | <br />                    |
| 0xC230 | k3                 | float64 LE | <br />                    |
| 0xC238 | k4                 | float64 LE | <br />                    |
| 0xC240 | reprojection error | float64 LE | <br />                    |
| 0xC300 | SN                 | ASCII × 20 | 例: `TMMX03CE022511130001` |

OpenCV-Fisheye 输出: `D = [k1, k2, k3, k4]` (4 项)

### H120 / H100 / H69 / H64 / H30 (Pinhole) — 13 字段

| 地址     | 字段                 | 类型 / 字节    | 备注                        |
| ------ | ------------------ | ---------- | ------------------------- |
| 0xC100 | software version   | ASCII × 6  | 例: `TMV1.1`               |
| 0xC106 | Model ID           | hex × 1    | 0x02..0x06                |
| 0xC200 | fx                 | float64 LE | <br />                    |
| 0xC208 | fy                 | float64 LE | <br />                    |
| 0xC210 | cx                 | float64 LE | <br />                    |
| 0xC218 | cy                 | float64 LE | <br />                    |
| 0xC220 | k1                 | float64 LE | <br />                    |
| 0xC228 | k2                 | float64 LE | <br />                    |
| 0xC230 | k3                 | float64 LE | <br />                    |
| 0xC238 | k4                 | float64 LE | <br />                    |
| 0xC240 | k5                 | float64 LE | <br />                    |
| 0xC248 | k6                 | float64 LE | <br />                    |
| 0xC250 | p1                 | float64 LE | <br />                    |
| 0xC258 | p2                 | float64 LE | <br />                    |
| 0xC260 | reprojection error | float64 LE | <br />                    |
| 0xC300 | SN                 | ASCII × 20 | 例: `TMMX03CE022511130001` |

OpenCV-Pinhole 输出: `D = [k1, k2, p1, p2, k3]` (5 项)

### IEEE754 double little-endian 解析示例

```c
typedef union {
    double a;
    char   b[8];
} doublePacket;

doublePacket test;
test.b[0] = 0xa1; test.b[1] = 0x58; test.b[2] = 0xd2; test.b[3] = 0x35;
test.b[4] = 0x7c; test.b[5] = 0x11; test.b[6] = 0x7c; test.b[7] = 0x40;
printf("test = %f\n", test.a);   /* = 29.689... */
```

## 代码目录结构

| 文件                         | 作用                                                               |
| -------------------------- | ---------------------------------------------------------------- |
| `isx031_flash.h`           | 协议常量、内参布局、模型枚举、API 声明                                            |
| `isx031_flash.c`           | I2C 底层 + Flash 协议 + 内参打包/解包 + meta 读取                            |
| `isx031_intrinsics_tool.c` | CLI 工具 (`read` / `write` / `dump` / `erase` / `scan` / `verify`) |
| `Makefile`                 | 编译脚本 (ARM64 交叉编译 / Jetson 本机编译)                                  |

## Flash 访问约束

| # | 约束                                                | 工具内的处理                                                  |
| - | ------------------------------------------------- | ------------------------------------------------------- |
| 1 | 一次读/写最大 **90 字节** (Sensor buffer 大小)              | `isx031_flash_read` / `_write` 内部按 ≤90B 自动分片            |
| 2 | 命令包 byte\[7] 固定填 **0x5A** (Sensor 固件 magic)       | `isx031_flash_send_cmd` 强制使用 `ISX031_CMD_PKT_LEN_MAGIC` |
| 3 | Flash 擦除以 **Sector** 为单位,1 Sector = **4096 Byte** | `isx031_flash_sector_erase` + 写前自动预擦除                   |
| 4 | 4 Mbit 区域无 Reserve Area,存内参需选 8 Mbit 或更大 Flash    | 内参区起始 `0xC200`,H200 占 72 字节,Pinhole 占 104 字节            |
| 5 | 写入前必须先擦除已有数据的目标 Sector                            | `isx031_flash_write` 自动 `collect_sectors` + 逐 Sector 擦除 |

## 协议要点 (与 i2ctransfer 实测一致)

```bash
# 1) 解锁 (两次 raw 写)
i2ctransfer -y -f 9 w3@0x3a 0xFF 0xFF 0xF4
i2ctransfer -y -f 9 w3@0x3a 0xFF 0xFF 0xF7

# 2) 命令包 (8 字节 raw 写, byte[7] 固定 0x5A)
i2ctransfer -y -f 9 w8@0x3a 0x80 0x00 CMD 0x00 ADDR[2] ADDR[1] ADDR[0] 0x5A
#   CMD=0x01 读 / 0x02 写 / 0x03 擦
#   ADDR 24-bit 大端
#   byte[7] 必须是 0x5A, 不能填实际要读的字节数 (填别的 Sensor 返回全 0)

# 3) 数据端口 (一次 i2c_rdwr: 写触发 + 读 N 字节)
i2ctransfer -y -f 9 w2@0x3a 0x00 0x00 r64
```

## 编译

### Jetson Orin 本机编译

```bash
make CROSS_COMPILE=
```

### ARM64 交叉编译 (推荐用于本仓库)

```bash
make            # 默认使用 /opt/gcc-linaro-11.3_aarch64-linux-gnu/...
make CROSS_COMPILE=/path/to/aarch64-linux-gnu-    # 自定义
```

默认交叉编译器路径:

```
/opt/gcc-linaro-11.3_aarch64-linux-gnu/bin/aarch64-buildroot-linux-gnu-
```

### 清理

```bash
make clean
```

## 用法

```bash
./isx031_intrinsics_tool <i2c_dev> <command> [options]
```

### 通用选项

| 选项                      | 含义                                                                     |
| ----------------------- | ---------------------------------------------------------------------- |
| `--addr ADDR[,ADDR...]` | 7-bit I2C 地址,逗号分隔,如 `0x3A,0x3B`                                        |
| `--all-default`         | 使用默认解串器地址 `0x3A..0x3D`                                                 |
| `--auto`                | 从 Sensor 的 Model ID (0xC106) 自动检测型号 (推荐)                               |
| `--model MODEL`         | 显式指定型号: `h200` \| `h69` \| `h120` \| `h64` \| `h100` \| `h30` 或 `1..6` |
| `--verbose`             | 打印所有 I2C 报文字节级细节 (调试用)                                                 |

### `read` — 读取内参

```bash
# 单 Sensor, OpenCV YAML
sudo ./isx031_intrinsics_tool /dev/i2c-9 read --addr 0x3A --format opencv-yaml -o cam.yaml

# 多 Sensor, 自动检测型号, JSON 输出
sudo ./isx031_intrinsics_tool /dev/i2c-9 read --auto --all-default --format json -o all.json

# 显式指定 H200 (Fisheye), opencv-yaml 自动用 4 项 D
sudo ./isx031_intrinsics_tool /dev/i2c-9 read --model h200 --addr 0x3A --format opencv-yaml

# 显式指定 H120 (Pinhole)
sudo ./isx031_intrinsics_tool /dev/i2c-9 read --model h120 --addr 0x3A --format opencv-yaml

# 附加 CRC16-CCITT
sudo ./isx031_intrinsics_tool /dev/i2c-9 read --auto --addr 0x3A --format json --crc
```

JSON 输出示例 (单 Sensor):

```json
{
  "0x3A": {
    "model": "H120",
    "model_id": "0x03",
    "sw_version": "TMV1.1",
    "sn": "TMMX03CE022511130001",
    "base": "0xC200",
    "raw_hex": "0000...",
    "crc16": "0xABCD",
    "intrinsics": {
      "fx": 1234.5, "fy": 1230.2, "cx": 960.0, "cy": 540.0,
      "k1": -0.10, "k2": 0.05, "k3": -0.01, "k4": 0.0,
      "k5": 0.0, "k6": 0.0,
      "p1": 0.0001, "p2": -0.0002,
      "repro": 0.35
    }
  }
}
```

H200 (Fisheye) JSON 示例:

```json
{
  "0x3A": {
    "model": "H200",
    "model_id": "0x01",
    "sw_version": "TMV1.1",
    "sn": "TMMX...",
    "intrinsics": {
      "fx": ..., "fy": ..., "cx": ..., "cy": ...,
      "k1": ..., "k2": ..., "k3": ..., "k4": ...,
      "repro": ...
    }
  }
}
```

OpenCV YAML 输出示例 (Pinhole):

```yaml
0x3A:
  model: H120
  sw_version: TMV1.1
  sn: TMMX03CE022511130001
  K: !!opencv-matrix
    rows: 3
    cols: 3
    dt: d
    data: [1234.5, 0., 960., 0., 1230.2, 540., 0., 0., 1.]
  D: !!opencv-matrix
    rows: 1
    cols: 5
    dt: d
    data: [-0.10, 0.05, 0.0001, -0.0002, -0.01]
```

OpenCV YAML 输出示例 (H200 Fisheye):

```yaml
0x3A:
  model: H200
  K: !!opencv-matrix
    rows: 3
    cols: 3
    dt: d
    data: [fx, 0., cx, 0., fy, cy, 0., 0., 1.]
  D: !!opencv-matrix
    rows: 1
    cols: 4
    dt: d
    data: [k1, k2, k3, k4]
```

Pinhole YAML 可直接被 OpenCV `cv::FileStorage` + `cv::initUndistortRectifyMap` 加载。
H200 YAML 适用于 `cv::fisheye::initUndistortRectifyMap`。

### `write` — 写入内参

```bash
# 单字段写入并回读校验
sudo ./isx031_intrinsics_tool /dev/i2c-9 write --addr 0x3A \
    --field fx --value 1234.5 --verify

# 自动检测型号,整段从 YAML 写入
sudo ./isx031_intrinsics_tool /dev/i2c-9 write --auto --all-default \
    --from-yaml batch.yaml --verify

# 显式按 H200 模型写入
sudo ./isx031_intrinsics_tool /dev/i2c-9 write --model h200 --addr 0x3A \
    --from-yaml h200.yaml
```

`batch.yaml` (Pinhole 模型) 样例:

```yaml
fx: 1234.5
fy: 1230.2
cx: 960.0
cy: 540.0
k1: -0.10
k2: 0.05
k3: -0.01
k4: 0.0
k5: 0.0
k6: 0.0
p1: 0.0001
p2: -0.0002
repro: 0.35
```

`h200.yaml` (H200 Fisheye 模型) 样例 (无 k5/k6/p1/p2):

```yaml
fx: 850.0
fy: 850.0
cx: 960.0
cy: 540.0
k1: -0.12
k2: 0.04
k3: -0.005
k4: 0.0
repro: 0.42
```

### `dump` — 导出 raw 字节

```bash
# 导出 Pinhole 内参 (104 字节)
sudo ./isx031_intrinsics_tool /dev/i2c-9 dump \
    --addr 0x3A --start 0xC200 --len 104 -o int.bin

# 导出 sw_version + model_id (7 字节)
sudo ./isx031_intrinsics_tool /dev/i2c-9 dump \
    --addr 0x3A --start 0xC100 --len 7 -o meta.bin

# 导出 SN (20 字节)
sudo ./isx031_intrinsics_tool /dev/i2c-9 dump \
    --addr 0x3A --start 0xC300 --len 20 -o sn.bin
```

### `erase` — 擦除 Sector (4096B/个)

```bash
# 从 0xC000 起擦 4 个 Sector (16KB) — 同时覆盖内参区和 SN 区
sudo ./isx031_intrinsics_tool /dev/i2c-9 erase \
    --addr 0x3A --start 0xC000 --count 4
```

### `scan` — 扫描 I2C 总线

```bash
sudo ./isx031_intrinsics_tool /dev/i2c-9 scan
```

输出 JSON,包含 `found` / `addrs` / `default_map` 三个字段。

### `verify` — 回读校验

```bash
sudo ./isx031_intrinsics_tool /dev/i2c-9 verify \
    --addr 0x3A --expected-yaml expected.yaml
```

## 头文件 API 摘要

```c
#include "isx031_flash.h"

/* ============ 型号与元数据 ============ */
typedef enum {
    ISX031_MODEL_UNKNOWN = 0x00,
    ISX031_MODEL_H200    = 0x01,  /* Fisheye  */
    ISX031_MODEL_H69     = 0x02,  /* Pinhole  */
    ISX031_MODEL_H120    = 0x03,  /* Pinhole  */
    ISX031_MODEL_H64     = 0x04,  /* Pinhole  */
    ISX031_MODEL_H100    = 0x05,  /* Pinhole  */
    ISX031_MODEL_H30     = 0x06,  /* Pinhole  */
} isx031_model_t;

const char *isx031_model_name(isx031_model_t m);
const char *isx031_model_type(isx031_model_t m);   /* "fisheye" | "pinhole" */

typedef struct {
    char          sw_version[7];
    unsigned char model_id;
    isx031_model_t model;
    char          sn[21];
} isx031_meta_t;

isx031_model_t isx031_detect_model(int fd, unsigned char addr);
int            isx031_read_meta   (int fd, unsigned char addr, isx031_meta_t *out);

/* ============ 内参结构 ============ */
typedef struct {
    double fx, fy, cx, cy;
    double k1, k2, k3, k4, k5, k6;
    double p1, p2;
    double repro;
} isx031_intrinsics_t;

/* ============ I2C 底层 ============ */
int  isx031_i2c_open(const char *dev_path);
int  isx031_i2c_raw_write(int fd, unsigned char addr, const void *p, unsigned int n);
int  isx031_i2c_raw_write_then_read(int fd, unsigned char addr,
                                    const void *wp, unsigned int wn,
                                    void *rbuf, unsigned int rn);
int  isx031_i2c_scan(int fd, unsigned char *out, int out_cap);

/* ============ Flash 协议 ============ */
int  isx031_flash_unlock       (int fd, unsigned char addr);
int  isx031_flash_read_chunk   (int fd, unsigned char addr, uint32_t addr24,
                                unsigned char *buf, unsigned int len);
int  isx031_flash_write_chunk  (int fd, unsigned char addr, uint32_t addr24,
                                const unsigned char *buf, unsigned int len);
int  isx031_flash_sector_erase (int fd, unsigned char addr, uint32_t addr24);
int  isx031_flash_wait_erase_done(int fd, unsigned char addr, unsigned int timeout_ms);
int  isx031_flash_read (int fd, unsigned char addr, uint32_t addr24,
                        unsigned int length, unsigned char *buf);
int  isx031_flash_write(int fd, unsigned char addr, uint32_t addr24,
                        unsigned int length, const unsigned char *buf);

/* ============ 内参高级 API (推荐入口) ============ */
int  isx031_read_intrinsics (int fd, unsigned char addr, isx031_model_t m,
                             isx031_intrinsics_t *out,
                             unsigned char *raw_out, unsigned int raw_cap);
int  isx031_read_all        (int fd, unsigned char addr,
                             isx031_model_t *out_model,
                             isx031_meta_t *meta,         /* 可为 NULL */
                             isx031_intrinsics_t *intr);  /* 可为 NULL */
int  isx031_write_field     (int fd, unsigned char addr,
                             isx031_field_t f, double v);
int  isx031_write_intrinsics(int fd, unsigned char addr, isx031_model_t m,
                             const isx031_intrinsics_t *v);

/* ============ 字段打包/解包 ============ */
void   isx031_pack_field  (isx031_field_t f, double v, unsigned char out[8]);
double isx031_unpack_field(const unsigned char in[8]);
void   isx031_parse_intrinsics(const unsigned char *raw, unsigned int raw_len,
                               isx031_model_t m, isx031_intrinsics_t *out);

/* ============ 校验 ============ */
unsigned short isx031_crc16_ccitt(const unsigned char *data, unsigned int len,
                                  unsigned short seed);
```

## 嵌入式 / 量产集成示例

```c
#include "isx031_flash.h"

/* 一站式: 探测型号 + 读 meta + 读内参 */
static int load_intrinsics(int fd, unsigned char sensor_addr,
                           isx031_meta_t *meta, isx031_intrinsics_t *K)
{
    isx031_model_t m;
    if (isx031_read_all(fd, sensor_addr, &m, meta, K) < 0) {
        fprintf(stderr, "read failed\n");
        return -1;
    }
    fprintf(stderr, "Sensor: %s (%s), SW=%s, SN=%s\n",
            isx031_model_name(m), isx031_model_type(m),
            meta->sw_version, meta->sn);
    /* 合理性检查: fx/fy 异常大通常意味着大小端或协议错 */
    if (K->fx > 1e6 || K->fy > 1e6) {
        fprintf(stderr, "warning: fx/fy suspicious, check endianness\n");
    }
    return 0;
}

/* 按探测到的型号写入内参 */
static int store_intrinsics(int fd, unsigned char sensor_addr,
                            const isx031_intrinsics_t *K)
{
    isx031_model_t m = isx031_detect_model(fd, sensor_addr);
    if (m == ISX031_MODEL_UNKNOWN) {
        fprintf(stderr, "model detect failed\n");
        return -1;
    }
    return isx031_write_intrinsics(fd, sensor_addr, m, K);
}
```

## 注意事项

- 必须使用 `sudo` 运行以获得 `/dev/i2c-*` 访问权限
- Linux 内核需开启 `I2C_RDWR` 支持 (默认开启)
- 写入为破坏性操作,请确保传入的内参值已校验
- 单条命令 byte\[7] 必须填 **0x5A** (协议 magic),不能填实际字节数
- 单条命令实际传输上限 **90 字节** (Sensor 内部 buffer 大小)
- 104 字节内参自动按 90+14 分两次命令读取
- 内参数据是 IEEE-754 **double** (float64) 小端

## 目标平台

- Jetson Orin (ARM64 Linux, Ubuntu 22.04)
- 标准 Linux I2C 子系统 (`/dev/i2c-N` + `ioctl(I2C_RDWR)`)
- 无任何第三方依赖,仅依赖 glibc + Linux kernel headers

