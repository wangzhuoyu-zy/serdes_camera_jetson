# fzcam_sync 使用手册

## 1. 环境要求

- 硬件: NVIDIA Jetson AGX Orin
- 系统: JetPack 6.2 (Linux 5.15-tegra)
- 设备树中已配置 `fzcam_sync` 节点 (compatible `"fz,fzcam_sync"`)

## 2. 编译

### 2.1 本机编译 (Jetson 上)

```bash
cd ~/fzcam_sync
make build      # 编译内核模块
make daemon     # 编译守护进程
make all        # 全部编译
```

### 2.2 交叉编译 (x86 主机 → Jetson)

在 x86 主机上交叉编译时，内核源码树通常是只读快照，**会缺少 `scripts/module.lds`**，需要先执行 `prepare-kernel` 目标自动补齐：

```bash
cd ~/fzcam_sync

# 一次性：生成 module.lds (从 .module.lds.cmd 中提取正确的 gcc 命令)
CROSS=1 make prepare-kernel

# 编译
CROSS=1 make build      # 编译内核模块
CROSS=1 make daemon     # 编译守护进程
```

也可以直接使用一键脚本（推荐）：

```bash
./build_kernel_modules.sh
```

脚本会：
1. 检查 `.config` 和交叉工具链（默认 `gcc-linaro-11.3_aarch64-linux-gnu`）
2. 调用 `make modules_prepare`（若内核树只读不会致命）
3. 通过 `scripts/.module.lds.cmd` 中的命令生成 `module.lds`（如已存在则跳过）
4. 依次构建 `.ko` 和 daemon

### 2.3 编译产物

| 文件 | 路径 |
|------|------|
| 内核模块 | `build/fzcam_sync.ko` |
| 守护进程 | `daemon/build/fzcam_sync_daemon` |

可用 `file <path>` 验证产物架构：

```bash
$ file build/fzcam_sync.ko
build/fzcam_sync.ko: ELF 64-bit LSB relocatable, ARM aarch64, ...

$ file daemon/build/fzcam_sync_daemon
daemon/build/fzcam_sync_daemon: ELF 64-bit LSB pie executable, ARM aarch64, ...
```

### 2.4 自定义路径

```bash
# 自定义内核目录
KDIR=/path/to/kernel-jammy-src CROSS=1 make build

# 自定义工具链
TOOLCHAIN_BASE=/opt/my-toolchain CROSS=1 make build

# 自定义 JetPack 版本号 (仅用于日志输出)
JP=6.2 CROSS=1 make build
```

### 2.5 故障：scripts/module.lds 缺失

如果看到：

```
make[3]: *** No rule to make target 'scripts/module.lds', needed by '.../fzcam_sync.ko'.  Stop.
```

说明内核源码树没有 `scripts/module.lds`。修复方法（任选一种）：

```bash
# 方案 A: 使用 Makefile 目标
CROSS=1 make prepare-kernel

# 方案 B: 手动执行 .cmd 中记录的精确命令
cd /path/to/kernel-jammy-src
head -1 scripts/.module.lds.cmd | sed 's/^cmd_scripts\/module.lds := //' | bash
```

## 3. 加载驱动

```bash
# 加载内核模块
sudo insmod build/fzcam_sync.ko

# 确认加载成功
dmesg | tail -5
# 预期输出: fzcam_sync probe success, /dev/fzcam_sync ready

# 检查设备节点
ls -la /dev/fzcam_sync
# 预期输出: crw------- 1 root root 10, xxx ... /dev/fzcam_sync
```

### 3.1 检查 debugfs (可选)

```bash
cat /sys/kernel/debug/fzcam_sync/status
# running:      no
# ts_capture:   stopped
# ts_sequence:  0
# ts_queue:     0/32
# generators:   1
# period_ns:    0

cat /sys/kernel/debug/fzcam_sync/timestamp/count
# 0
```

## 4. 配置频率

编辑配置文件:

```bash
sudo mkdir -p /etc/fzcam_sync
sudo cp config/fzcam_sync.conf /etc/fzcam_sync/
sudo nano /etc/fzcam_sync/fzcam_sync.conf
```

配置文件格式 (`/etc/fzcam_sync/fzcam_sync.conf`):

```
# FPS: frames per second * 100
#   Valid range: 100 ~ 9000 (1.00 ~ 90.00 Hz)
FPS=3000

# DUTY_CYCLE: PWM duty cycle in percent
#   Valid range: 1 ~ 99
DUTY_CYCLE=10
```

| FPS 值 | 对应频率 | FPS 值 | 对应频率 |
|--------|----------|--------|----------|
| 100 | 1 Hz | 3000 | 30 Hz |
| 500 | 5 Hz | 6000 | 60 Hz |
| 1000 | 10 Hz | 7500 | 75 Hz |
| 1500 | 15 Hz | 9000 | 90 Hz |
| 2000 | 20 Hz | | |

## 5. 运行守护进程

### 5.1 直接运行

```bash
sudo ./daemon/build/fzcam_sync_daemon

# 预期输出 (stderr):
# fzcam_sync_daemon: starting
#   config:    /etc/fzcam_sync/fzcam_sync.conf
#   FPS:       3000 (30.00 Hz)
#   duty_cycle: 10%
# fzcam_sync_daemon: TSC started, reading timestamps...
#   generators: 1
```

### 5.2 时间戳输出 (stdout)

```
[FZCAM_SYNC] seq=0 gen=0 edge=0 tsc=12345678901234 capture=123.456789012 flags=0x1
[FZCAM_SYNC] seq=1 gen=0 edge=0 tsc=12345682345678 capture=123.490122345 flags=0x1
[FZCAM_SYNC] seq=2 gen=0 edge=0 tsc=12345685790123 capture=123.523455678 flags=0x1
...
```

输出字段说明:

| 字段 | 含义 |
|------|------|
| `seq` | 时间戳序列号 (单调递增) |
| `gen` | 信号发生器 ID |
| `edge` | 边沿类型 (0=上升沿) |
| `tsc` | TSC 计数器纳秒值 (硬件时钟域) |
| `capture` | CLOCK_MONOTONIC 捕获时刻 (秒.纳秒) |
| `flags` | 标志位 (0x1=VALID) |

### 5.3 停止

按 `Ctrl+C` 或发送 `SIGTERM`:

```bash
sudo killall fzcam_sync_daemon
```

预期退出输出:
```
fzcam_sync_daemon: stopping TSC...
fzcam_sync_daemon: stopped.
```

## 6. 注册为系统服务 (可选)

```bash
# 安装二进制
sudo cp daemon/build/fzcam_sync_daemon /usr/local/bin/

# 安装配置
sudo mkdir -p /etc/fzcam_sync
sudo cp config/fzcam_sync.conf /etc/fzcam_sync/

# 安装 systemd 服务
sudo cp service/fzcam-sync.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable fzcam-sync
sudo systemctl start fzcam-sync

# 查看服务状态
systemctl status fzcam-sync

# 查看日志输出
journalctl -u fzcam-sync -f
```

## 7. 验证

### 7.1 检查 TSC 信号 (硬件验证)

用示波器连接 TSC 信号输出引脚 (通常为 GPIO50 / J6 Pin2):

- 应看到 PWM 方波信号
- 频率 = 配置的 FPS 值 (例如 30Hz)
- 占空比 = 配置的 DUTY_CYCLE 值 (例如 10%)

### 7.2 检查时间戳连续性

验证 `tsc` 字段单调递增，相邻间隔应等于帧周期:

```
30Hz: 约 33,333,333 ns
60Hz: 约 16,666,667 ns
90Hz: 约 11,111,111 ns
```

```bash
# 运行守护进程，观察输出
sudo ./daemon/build/fzcam_sync_daemon 2>/dev/null | head -10
```

### 7.3 通过 debugfs 实时查看

```bash
# 查看队列中的时间戳数量
watch -n 0.5 cat /sys/kernel/debug/fzcam_sync/timestamp/count

# 查看最新时间戳
cat /sys/kernel/debug/fzcam_sync/timestamp/latest

# 查看完整状态
cat /sys/kernel/debug/fzcam_sync/status
```

### 7.4 内核日志验证

```bash
dmesg | grep fzcam_sync
```

预期输出示例:
```
fzcam_sync probe success, /dev/fzcam_sync ready
Generator 0: freq=3007000 duty=10% ts=enabled
Starting TSC generators...
Generator 0: timestamp interrupt enabled
Generator 0 started
Timestamp capture started, period=33333333ns
Started: fps=30.00 duty=10% ts=on
```

## 8. 卸载

```bash
# 停止守护进程 (如果使用 systemd)
sudo systemctl stop fzcam-sync

# 卸载内核模块
sudo rmmod fzcam_sync

# 确认已卸载
lsmod | grep fzcam_sync
# 应无输出
```

## 9. 故障排查

| 问题 | 排查步骤 |
|------|----------|
| 设备节点 `/dev/fzcam_sync` 不存在 | 检查设备树是否配置了 `fz,fzcam_sync` 节点; `dmesg \| grep fzcam_sync` 查看 probe 日志 |
| 守护进程无法打开设备 | `ls -la /dev/fzcam_sync`; 确认模块已加载且不是 `fz_cam_sync` 占用 |
| 频率不准确 | 检查配置文件 FPS 值是否在 100~9000 范围内; `adjust_gen_freq_for_fps` 中的 offset_us 可能需要根据实际硬件调优 |
| 没有时间戳输出 | 确认启动时 `enable_timestamp` 为 1; 检查 `cat /sys/kernel/debug/fzcam_sync/status` |
| `TSC started` 但无时间戳 | 用示波器确认 TSC 信号有输出; 检查 `debug_en=1` 加载模块后 `dmesg` 中的 `TS:` 日志 |
| 与现有 `fz_cam_sync` 冲突 | 两个模块使用不同的 compatible 和设备节点，可共存 |
