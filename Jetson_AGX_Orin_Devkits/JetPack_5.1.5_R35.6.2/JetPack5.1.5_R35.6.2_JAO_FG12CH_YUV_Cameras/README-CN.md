# FG12CH YUV Cameras JetPack 5.1.5 (R35.6.2) SOP

目录：JetPack5.1.5_R35.6.2_JAO_FG12CH_YUV_Cameras

## 1. 目标

- 在 Jetson AGX Orin + FG12CH YUV Cameras 套件上完成固件/应用升级。
- 支持 JetPack 5.1.5 / R35.6.2 内核（Linux 5.10.216-tegra）。
- 支持 12 通道 YUV 相机的配置和使用。

## 2. 升级包内容说明

- 升级脚本
  - `fg24-12ch.JAO.R35.6.2.sh`
- 驱动与 DTB
  - `kernel/fzcam.ko` — 相机驱动
  - `kernel/dtb/tegra234-p3701-0000-p3737-0000.dtb`
  - `kernel/dtb/tegra234-p3701-0004-p3737-0000.dtb`
  - `kernel/dtb/tegra234-p3701-0005-p3737-0000.dtb`
- 应用与配置
  - `fzcam_app/usr/local/bin/fzcam_ui` — QT 界面程序
  - `fzcam_app/usr/local/bin/fzcam_cfg` — 相机/Serdes 配置工具
  - `fzcam_app/etc/fzcam_cfg.ini` — 相机链接配置文件
  - `fzcam_app/fzcam_cfg.service` — systemd 服务

## 3. 在 Jetson 上执行升级

将整个目录拷贝到 Jetson（任选一种方式）：

```bash
scp -r JetPack5.1.5_R35.6.2_JAO_FG12CH_YUV_Cameras nvidia@<JETSON_IP>:
```

在 Jetson 上进入目录并执行升级脚本（需要 sudo）：

```bash
cd ~/JetPack5.1.5_R35.6.2_JAO_FG12CH_YUV_Cameras
sudo bash fg24-12ch.JAO.R35.6.2.sh
```

脚本做的事情（关键点）：
- 检查 `/etc/nv_tegra_release` 是否为 R35.6.2，不匹配则退出
- 安装配置文件 `/etc/fzcam_cfg.ini`
- 安装应用：
  - `/usr/local/bin/fzcam_cfg`
  - `/usr/local/bin/fzcam_ui`
- 安装并启用 systemd 服务：`/etc/systemd/system/fzcam_cfg.service`
- 安装驱动 `fzcam.ko` 到 `/lib/modules/5.10.216-tegra/kernel/drivers/media/i2c/` 并 `insmod` / `depmod`
- 备份并替换 DTB 文件（根据当前硬件自动选择对应 SKU 的 dtb）
- 二次确认后重启

## 4. 重启后配置与出图验证

### 4.1 运行 UI 配置

```bash
sudo fzcam_ui
```

按 UI 选择厂商/型号后：
- 点击"保存配置"
- 再点击"运行配置"
- 观察 Link status 是否为 1（表示链路已锁定并有视频数据）

### 4.2 GStreamer 快速验证

以 video0（示例）：

```bash
gst-launch-1.0 v4l2src device=/dev/video0 ! 'video/x-raw,format=UYVY,width=1920,height=1080' ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
```

### 4.3 设备节点检查

```bash
uname -r
# 预期: 5.10.216-tegra

lsmod | grep fzcam
# 预期: fzcam <size> 0

ls -l /dev/video*
# 预期: 多个 /dev/videoX 节点
```

## 5. 常见问题

### 5.1 JetPack 版本不匹配

脚本会检查 `/etc/nv_tegra_release` 是否为 R35.6.2，若不匹配会退出。请确认当前系统为 JetPack 5.1.5。

### 5.2 DTB 替换后系统无法启动

如果替换 DTB 后系统异常，可以使用备份恢复：
```bash
sudo cp /boot/dtb/kernel_tegra234-p3701-0000-p3737-0000.dtb.orig \
        /boot/dtb/kernel_tegra234-p3701-0000-p3737-0000.dtb
sudo reboot
```

### 5.3 无视频节点/无图

表现：没有视频节点或无法出图。
处理：
1. 确认相机连接正确
2. 重新执行升级脚本并重启
3. 检查驱动是否加载：`lsmod | grep fzcam`
