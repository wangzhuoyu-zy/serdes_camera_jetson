# 335Lg JAO FG SERDES Driver Package

[中文](#中文说明) | [English](#english)

---

## 中文说明

### 1. 概述

本目录用于在 Jetson 开发板目标机上部署 335Lg + JAO + FG SERDES 相关驱动与设备树覆盖文件。

适用内容包括：

- JetPack `6.2.2`
- L4T / Jetson Linux `R36.5`
- 335Lg 相机
- JAO 平台
- FG96 8 路 / FG12-16CH 多路 SERDES 配置

该目录中包含：

- 内核模块 `.ko`
- 设备树覆盖文件 `.dtbo`
- 目标机安装脚本 `.sh`

安装脚本会将文件复制到目标 Jetson 系统中的对应目录，并调用 `jetson-io` 完成相机硬件配置。

### 2. 文件说明

主要文件如下：

- `copy_335Lg_to_JAO.fg96-8ch.sh`
  - 用于 FG96 8 路方案安装
- `copy_335Lg_to_JAO.fg12-16ch.sh`
  - 用于 FG12-16CH 方案安装
- `tegra-camera.ko`
  - Tegra camera 核心驱动替换文件
- `videodev.ko`
  - V4L2 核心模块，脚本中用于启用 metadata 相关支持
- `capture-ivc.ko`
  - RTCPU capture IVC 相关模块
- `nvhost-nvcsi-t194.ko`
  - NVCSI 相关模块
- `obc_max9296.ko`
  - MAX9296 驱动
- `obc_max96712.ko`
  - MAX96712 驱动
- `g300.ko`
  - G300 相关驱动
- `obc_cam_sync.ko`
  - 相机同步相关模块
- `tegra234-p3737-camera-g300-fg96-overlay.dtbo`
  - FG96 方案设备树覆盖文件
- `tegra234-p3737-camera-g300-fg12-16ch-overlay.dtbo`
  - FG12-16CH D-PHY 纯 335Lg 方案
- `tegra234-p3737-camera-g300-rgb-fg12-16ch-overlay.dtbo`
  - FG12-16CH 335Lg + RGB 方案
- `tegra234-p3737-camera-g300-fg12-16ch-cphy-overlay.dtbo`
  - FG12-16CH C-PHY 方案

### 3. 安装前准备

1. 将本目录完整拷贝到目标 Jetson 开发板。
2. 确认目标机系统版本与本包匹配：
   - JetPack `6.2.2`
   - 内核版本应与本驱动包一致
3. 建议先备份当前系统和重要驱动文件。
4. 需要具备 `sudo` 权限。

如果本包是压缩包形式，先解压：

```bash
unzip <package>.zip
cd 335Lg_JAO_FG_SERDES_Driver_JP6.2.2_R36.5
```

### 4. 安装步骤

#### 4.1 FG12-16CH 方案

在目标机目录中执行：

```bash
cd 335Lg_JAO_FG_SERDES_Driver_JP6.2.2_R36.5
sudo bash copy_335Lg_to_JAO.fg12-16ch.sh
```

脚本执行过程中会提示选择模式：

- `0`：CAM0~8 仅连接 335Lg 相机
- `1`：CAM0~8 仅连接 335Lg 相机（DEPTH/RGB）
- `2`：CAM0~8 仅连接 335Lg，且 FG12-16CH 使用 C-PHY

随后脚本会根据选择调用以下 `jetson-io` 配置之一：

- `Jetson Camera FG12_16CH_DPHY_G335Lg`
- `Jetson Camera FG12_16CH_DPHY_G335Lg_RGB`
- `Jetson Camera FG12_16CH_CPHY_G335Lg`

#### 4.2 FG96 8CH 方案

在目标机目录中执行：

```bash
cd 335Lg_JAO_FG_SERDES_Driver_JP6.2.2_R36.5
sudo bash copy_335Lg_to_JAO.fg96-8ch.sh
```

脚本执行过程中会提示选择模式：

- `0`：CAM0~7 仅连接 335Lg 相机
- `1`：CAM0~1 连接 335Lg，CAM2~7 连接 RGB
- `2`：CAM0~3 连接 335Lg，CAM4~7 连接 RGB

随后脚本会根据选择调用以下 `jetson-io` 配置之一：

- `Jetson Camera FG96_8CH_DPHY_8xG335Lg`
- `Jetson Camera FG96_8CH_2x335Lg_6xRGB`
- `Jetson Camera FG96_8CH_4x335Lg_4xRGB`

### 5. RGB 相机相关说明

两份安装脚本都会在后续询问是否已接入 RGB 相机：

```bash
是否已接入RGB相机？(y/N)
```

如果输入 `y` 或 `Y`，脚本还会额外部署以下内容：

- `fzcam.ko`
- `fzcam_app/etc/fzcam_cfg.ini`
- `fzcam_app/usr/local/bin/fzcam_cfg`
- `fzcam_app/usr/local/bin/fzcam_ui`
- `fzcam_app/fzcam_cfg.service`

并执行：

- 安装 systemd 服务
- `systemctl daemon-reload`
- `depmod`
- 写入 `softdep fzcam pre: g300` 到 `/etc/modprobe.d/g300_depend.conf`

注意：当前目录列表中未看到 `fzcam.ko` 与 `fzcam_app` 目录。如果需要启用 RGB 相关功能，请确认这些文件已随安装包一并提供，否则脚本执行到对应步骤时可能失败。

### 6. 脚本实际做的事情

安装脚本主要执行以下操作：

- 备份原始模块到 `.orig`
- 覆盖目标系统中的驱动模块
- 复制 `.dtbo` 到 `/boot/`
- 调用 `/opt/nvidia/jetson-io/config-by-hardware.py`
- 在需要时安装 RGB 相关扩展模块与服务

涉及的典型目标路径包括：

- `/lib/modules/$(uname -r)/updates/drivers/media/platform/tegra/camera`
- `/lib/modules/$(uname -r)/kernel/drivers/media/v4l2-core`
- `/lib/modules/$(uname -r)/updates/drivers/platform/tegra/rtcpu`
- `/lib/modules/$(uname -r)/updates/drivers/video/tegra/host/nvcsi/`
- `/lib/modules/$(uname -r)/updates/drivers/media/i2c/`
- `/lib/modules/$(uname -r)/updates/drivers/misc/`
- `/boot/`

### 7. 安装后建议

安装完成后建议执行：

```bash
sudo depmod
sudo sync
sudo reboot
```

重启后再检查：

- 相关模块是否加载成功
- `/boot/` 中 overlay 是否正确部署
- `jetson-io` 配置是否生效
- 相机节点是否正常出现

### 8. 注意事项

- 本驱动包与 JetPack / L4T 版本强相关，不建议跨版本混用。
- 覆盖系统驱动前请确认已有备份。
- `copy_335Lg_to_JAO.fg96-8ch.sh` 中复制的 dtbo 文件名写为 `tegra234-camera-g300-fg96-8ch-overlay.dtbo`，而当前目录实际文件名为 `tegra234-p3737-camera-g300-fg96-overlay.dtbo`。如果脚本执行时报找不到该 dtbo 文件，请优先检查并修正文件名或脚本内容。
- 若 `jetson-io` 中不存在对应硬件配置项，请确认目标系统已集成相关硬件定义。

---

## English

### 1. Overview

This directory contains the driver package for deploying the 335Lg + JAO + FG SERDES camera solution on a target Jetson device.

Target environment:

- JetPack `6.2.2`
- L4T / Jetson Linux `R36.5`
- 335Lg camera modules
- JAO platform
- FG96 8-channel and FG12-16CH SERDES configurations

The package includes:

- Kernel modules (`.ko`)
- Device tree overlays (`.dtbo`)
- Installation scripts (`.sh`)

The installation scripts copy files into the proper system locations on the target Jetson device and then invoke `jetson-io` to apply the camera hardware configuration.

### 2. File Description

Main files in this package:

- `copy_335Lg_to_JAO.fg96-8ch.sh`
  - Installer for the FG96 8-channel configuration
- `copy_335Lg_to_JAO.fg12-16ch.sh`
  - Installer for the FG12-16CH configuration
- `tegra-camera.ko`
  - Tegra camera core driver replacement
- `videodev.ko`
  - V4L2 core module, used in the script to enable metadata-related support
- `capture-ivc.ko`
  - RTCPU capture IVC related module
- `nvhost-nvcsi-t194.ko`
  - NVCSI related module
- `obc_max9296.ko`
  - MAX9296 driver
- `obc_max96712.ko`
  - MAX96712 driver
- `g300.ko`
  - G300 related driver
- `obc_cam_sync.ko`
  - Camera synchronization related module
- `tegra234-p3737-camera-g300-fg96-overlay.dtbo`
  - Device tree overlay for the FG96 configuration
- `tegra234-p3737-camera-g300-fg12-16ch-overlay.dtbo`
  - FG12-16CH D-PHY, pure 335Lg configuration
- `tegra234-p3737-camera-g300-rgb-fg12-16ch-overlay.dtbo`
  - FG12-16CH 335Lg + RGB configuration
- `tegra234-p3737-camera-g300-fg12-16ch-cphy-overlay.dtbo`
  - FG12-16CH C-PHY configuration

### 3. Before Installation

1. Copy this entire directory to the target Jetson device.
2. Confirm the target system matches this package:
   - JetPack `6.2.2`
   - Matching kernel version
3. Back up the current system and important driver files if needed.
4. Make sure `sudo` is available.

If the package is provided as a zip archive, unpack it first:

```bash
unzip <package>.zip
cd 335Lg_JAO_FG_SERDES_Driver_JP6.2.2_R36.5
```

### 4. Installation

#### 4.1 FG12-16CH Configuration

Run on the target Jetson device:

```bash
cd 335Lg_JAO_FG_SERDES_Driver_JP6.2.2_R36.5
sudo bash copy_335Lg_to_JAO.fg12-16ch.sh
```

During execution, the script asks you to choose one of the following modes:

- `0`: CAM0~8 connected to 335Lg cameras only
- `1`: CAM0~8 connected to 335Lg cameras only (DEPTH/RGB)
- `2`: CAM0~8 connected to 335Lg cameras only, using C-PHY for FG12-16CH

The script then applies one of these `jetson-io` hardware profiles:

- `Jetson Camera FG12_16CH_DPHY_G335Lg`
- `Jetson Camera FG12_16CH_DPHY_G335Lg_RGB`
- `Jetson Camera FG12_16CH_CPHY_G335Lg`

#### 4.2 FG96 8CH Configuration

Run on the target Jetson device:

```bash
cd 335Lg_JAO_FG_SERDES_Driver_JP6.2.2_R36.5
sudo bash copy_335Lg_to_JAO.fg96-8ch.sh
```

During execution, the script asks you to choose one of the following modes:

- `0`: CAM0~7 connected to 335Lg cameras only
- `1`: CAM0~1 use 335Lg, CAM2~7 use RGB
- `2`: CAM0~3 use 335Lg, CAM4~7 use RGB

The script then applies one of these `jetson-io` hardware profiles:

- `Jetson Camera FG96_8CH_DPHY_8xG335Lg`
- `Jetson Camera FG96_8CH_2x335Lg_6xRGB`
- `Jetson Camera FG96_8CH_4x335Lg_4xRGB`

### 5. RGB Camera Support

Both scripts later ask:

```bash
是否已接入RGB相机？(y/N)
```

If you answer `y` or `Y`, the scripts additionally install:

- `fzcam.ko`
- `fzcam_app/etc/fzcam_cfg.ini`
- `fzcam_app/usr/local/bin/fzcam_cfg`
- `fzcam_app/usr/local/bin/fzcam_ui`
- `fzcam_app/fzcam_cfg.service`

They also:

- Install the systemd service
- Run `systemctl daemon-reload`
- Run `depmod`
- Append `softdep fzcam pre: g300` to `/etc/modprobe.d/g300_depend.conf`

Note: the current directory listing does not show `fzcam.ko` or the `fzcam_app` directory. If RGB support is required, make sure those files are included in the final package, otherwise the script may fail at that stage.

### 6. What The Scripts Do

The scripts mainly perform the following operations:

- Back up original modules as `.orig`
- Replace system kernel modules
- Copy `.dtbo` files into `/boot/`
- Call `/opt/nvidia/jetson-io/config-by-hardware.py`
- Optionally install RGB-related modules and services

Typical destination paths include:

- `/lib/modules/$(uname -r)/updates/drivers/media/platform/tegra/camera`
- `/lib/modules/$(uname -r)/kernel/drivers/media/v4l2-core`
- `/lib/modules/$(uname -r)/updates/drivers/platform/tegra/rtcpu`
- `/lib/modules/$(uname -r)/updates/drivers/video/tegra/host/nvcsi/`
- `/lib/modules/$(uname -r)/updates/drivers/media/i2c/`
- `/lib/modules/$(uname -r)/updates/drivers/misc/`
- `/boot/`

### 7. Recommended Post-Install Steps

After installation, it is recommended to run:

```bash
sudo depmod
sudo sync
sudo reboot
```

After reboot, verify:

- Required modules are loaded
- Overlay files are present under `/boot/`
- `jetson-io` configuration has taken effect
- Camera device nodes appear as expected

### 8. Notes

- This package is tightly coupled to the JetPack / L4T version. Do not mix it with other releases unless verified.
- Make sure original files are backed up before replacing system drivers.
- In `copy_335Lg_to_JAO.fg96-8ch.sh`, the script copies `tegra234-camera-g300-fg96-8ch-overlay.dtbo`, but the actual file in this directory is `tegra234-p3737-camera-g300-fg96-overlay.dtbo`. If the script reports that the dtbo file is missing, check and correct the filename or the script.
- If the expected hardware profile is missing from `jetson-io`, verify that the target image already includes the required camera hardware definitions.
