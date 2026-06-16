# FZCam Sync Timestamp Overlay (Multi-Camera Demo)

实时采集 1~16 路相机画面，并把 `fzcam_sync` 内核模块捕获的 PWM 触发
上升沿时间戳叠加到每路画面上，最终拼成一张多格大图进行实时预览。

---

## 1. 功能

| 能力 | 说明 |
|------|------|
| 多相机采集 | 同时打开 1..16 个 `/dev/videoN` 设备 |
| 触发时间戳捕获 | 通过 ioctl 从 `/dev/fzcam_sync` 读取内核 hrtimer 抓取的时间戳 |
| 3 种时间显示 | **UTC**、**本地系统时间**、**TSC 硬件计数器** 同时叠加在画面上 |
| 网格预览 | 自动按 `cols × rows` 拼成大画布；显示超过 1920×1080 时自动等比缩放 |
| 拍图保存 | 按 `s` / `p` 把整张大图保存为 PNG（保留全分辨率叠加） |
| 软退出 | 按 `q` / `ESC` 或 `Ctrl+C` 安全清理 |

每路画面底部叠加 3 行：

```
CAM0  Trigger #123  TSC=12345678901234ns      ← 黄色
UTC:   2026-06-12 14:30:05.123456789
Local: 2026-06-12 22:30:05.123456789
```

---

## 2. 运行环境

- **平台**：NVIDIA Jetson AGX Orin / JetPack 6.x（aarch64）
- **依赖**：
  - OpenCV 4.x（`libopencv-dev`，含 core / videoio / highgui / imgproc / imgcodecs）
  - GCC 9+ 支持 C++17
  - `fzcam_sync.ko` 内核模块（提供 `/dev/fzcam_sync` 设备）
  - `fzcam.ko` 相机驱动 + V4L2 设备（提供 `/dev/videoN`）
- **权限**：`/dev/fzcam_sync` 和 `/dev/video*` 需要 root 或 video 组访问

---

## 3. 目录结构

```
timestamp_overlay/
├── timestamp_overlay.cpp   # 主程序（C++17 / OpenCV 4）
├── Makefile                # 编译脚本
├── run.sh                  # 一键启动脚本
└── README.md               # 本文档
```

---

## 4. 编译

```bash
cd ~/fzcam_sync/timestamp_overlay
make
```

生成可执行文件 `timestamp_overlay`（约 30 KB，链接到 libopencv_*）。

清理：
```bash
make clean
```

---

## 5. 使用方法

### 5.1 一键启动（推荐）

```bash
sudo ./run.sh -N 4 -i 0 -w 1920 -h 1080
```

`run.sh` 自动完成：
1. 停止 `fzcam-sync` systemd daemon（避免 ioctl 冲突）
2. 加载 `fzcam_sync.ko` 内核模块（如果未加载）
3. 编译并启动 `timestamp_overlay`

### 5.2 直接启动

```bash
sudo ./timestamp_overlay -N 4 -i 0 -w 1920 -h 1080
```

### 5.3 命令行参数

| 短选项 | 长选项 | 默认 | 说明 |
|--------|--------|------|------|
| `-N` | `--num` | 1 | 相机数量（1..16） |
| `-i` | `--index` | 0 | 起始 `/dev/videoN` 索引 |
| `-w` | `--width` | 1920 | 单相机宽度 |
| `-h` | `--height` | 1080 | 单相机高度 |
| `-c` | `--cols` | 自动 | 网格列数（0=自动：1→1, 2-3→2, 4+→√N） |
| `-?` | `--help-only` | — | 显示帮助 |

### 5.4 典型示例

| 场景 | 命令 |
|------|------|
| 单相机（向后兼容） | `sudo ./run.sh -N 1 -i 0 -w 1920 -h 1080` |
| 4 路 1080p（2×2 网格） | `sudo ./run.sh -N 4 -i 0 -w 1920 -h 1080` |
| 4 路 2K 2.5K | `sudo ./run.sh -N 4 -i 0 -w 2592 -h 1944` |
| 8 路 1080p（4×2 网格） | `sudo ./run.sh -N 8 -i 0 -w 1920 -h 1080 -c 4` |
| 8 路 2K（3×3 网格） | `sudo ./run.sh -N 8 -i 0 -w 2592 -h 1944` |
| 查看帮助 | `sudo ./run.sh -?` |

### 5.5 交互按键

| 按键 | 动作 |
|------|------|
| `q` 或 `ESC` | 退出程序 |
| `s` 或 `p` | 拍图保存（整张大图） |
| `Ctrl+C` | 软退出（同 q） |

拍图保存的命名：
- 已有触发：`snap_multi_trig{序号}_{毫秒}.png`
- 暂无触发：`snap_multi_{毫秒}.png`

---

## 6. 工作原理

```
┌─────────────────┐    ioctl(FZCAM_SYNC_GET_TS_BATCH)
│ /dev/fzcam_sync │◄──────────────────────────────────┐
│  内核 hrtimer   │                                    │
│  + 32 槽环形缓冲 │                                    │
└─────────────────┘                                    │
                                                       │
┌─────────────────┐   cv::VideoCapture (V4L2)         │
│ /dev/video0..N  │◄──────────────┐                    │
│  fzcam.ko       │               │                    │
└─────────────────┘               │                    │
                                  │                    │
              ┌───────────────────┴────────────────────┘
              │         timestamp_overlay
              │  1. 每个相机抓一帧
              │  2. ioctl 拉取最新时间戳
              │  3. CLOCK_MONOTONIC + 启动偏移 → UTC
              │  4. CLOCK_REALTIME      → 本地时间
              │  5. TSC counter         → 硬件时钟
              │  6. 拼成网格大图，叠加文字
              │  7. cv::imshow 显示（超 1920×1080 自动缩放）
              └────────────────────────────────────────
```

### 6.1 时间戳来源

`fzcam_sync.ko` 在每次 PWM 上升沿的 hrtimer 回调里：

1. 读 `ktime_get_ns()`（CLOCK_MONOTONIC，纳秒） → `capture_timestamp_ns`
2. 读硬件 TSC counter（×32ns/tick） → `tsc_counter_ns`
3. 推入 spinlock 保护的 32 槽环形缓冲

用户态通过 `ioctl(fd, FZCAM_SYNC_GET_TS_BATCH, &batch)` 一次性弹出所有待取的时间戳。

### 6.2 UTC 时间计算

`capture_timestamp_ns` 是 **CLOCK_MONOTONIC**（系统启动至今的纳秒数），不是 Unix epoch。
程序启动时取 5 次平均计算偏移：

```
g_mono_to_wall_offset_ns = (CLOCK_REALTIME - CLOCK_MONOTONIC)
```

转换公式：

```
UTC 纳秒 = capture_timestamp_ns + g_mono_to_wall_offset_ns
```

---

## 7. 输出示例

### 7.1 终端日志

```
=== timestamp_overlay 启动脚本 ===
[1/3] 停止 fzcam-sync daemon...
  daemon 未运行或已停止
[2/3] 检查 fzcam_sync 内核模块...
  模块已加载
[3/3] 运行 timestamp_overlay...
[INFO] mono->wall offset = 1781257225123456789ns
[INFO] fzcam_sync timestamp capture started (20 Hz)
[INFO] CAM0  /dev/video0 已打开 (1920x1080)
[INFO] CAM1  /dev/video1 已打开 (1920x1080)
[INFO] CAM2  /dev/video2 已打开 (1920x1080)
[INFO] CAM3  /dev/video3 已打开 (1920x1080)
[INFO] 网格 2 x 2  画布 2160 x 3840
[INFO] 拍图已保存: snap_multi_trig12345_1781257225123ms.png (2160x3840)
```

### 7.2 画面叠加（每路相机底部）

```
┌────────────────────────────────────────────┐
│                                            │
│            相机画面                        │
│                                            │
│                                            │
│────────────────────────────────────────────│
│ CAM0  Trigger #12345  TSC=12345678901234ns │  ← 黄字
│ UTC:   2026-06-12 14:30:05.123456789       │
│ Local: 2026-06-12 22:30:05.123456789       │
└────────────────────────────────────────────┘
```

---

## 8. 常见问题

### Q1: 提示 `[ERROR] open /dev/fzcam_sync`
- 确认 `fzcam_sync.ko` 已加载：`lsmod | grep fzcam_sync`
- 加载模块：`sudo insmod /home/nvidia/fzcam_sync/build/fzcam_sync.ko`
- 确认 daemon 未在用：`sudo systemctl stop fzcam-sync`

### Q2: 提示 `[WARN] 无法打开 /dev/video0`
- 确认设备存在：`ls /dev/video*`
- 确认 fzcam.ko 已加载：`lsmod | grep fzcam`
- 确认 video 组权限：当前用户需在 `video` 组，或用 `sudo` 运行

### Q3: 显示 `Segmentation fault`
- 检查 OpenCV 是否带 GUI 后端：可执行 `cv::getBuildInformation()` 查 `GTK / Qt`
- 如果是 SSH 无 `$DISPLAY`，需设置 X11 forwarding 或用本地桌面

### Q4: 时间显示 1970
- 旧版本 bug。**当前版本**已通过启动时计算 `MONOTONIC → wall clock` 偏移自动修正

### Q5: 拍图文件太大
- 4K×4 网格的全分辨率 PNG 可达几十 MB。可加 `-c 2` 减小列数，或在程序内把 `cv::imwrite` 改成 `cv::imwrite(filename, canvas, {cv::IMWRITE_PNG_COMPRESSION, 9})`

---

## 9. 内核模块与设备依赖

| 设备 | 来源 | 加载命令 |
|------|------|----------|
| `/dev/fzcam_sync` | fzcam_sync.ko | `sudo insmod /home/nvidia/fzcam_sync/build/fzcam_sync.ko` |
| `/dev/video0..N` | fzcam.ko + V4L2 子设备 | `sudo insmod /home/nvidia/fzcam/build/fzcam.ko` |
| `/dev/i2c-9..12` | DESER I2C 总线 | 由 fzcam.ko 创建设备节点 |

### 9.1 fzcam_sync.ko 编译（可选）

```bash
cd /home/nvidia/fzcam_sync
make            # 生成 build/fzcam_sync.ko
```

---

## 10. 许可证

内部 demo 代码，仅供项目内使用。
