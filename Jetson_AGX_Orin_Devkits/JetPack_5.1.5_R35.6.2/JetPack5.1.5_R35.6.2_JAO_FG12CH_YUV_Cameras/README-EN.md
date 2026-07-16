# FG12CH YUV Cameras JetPack 5.1.5 (R35.6.2) SOP

Directory: JetPack5.1.5_R35.6.2_JAO_FG12CH_YUV_Cameras

## 1. Objectives

- Complete firmware/application upgrade on Jetson AGX Orin + FG12CH YUV Cameras kit.
- Supports JetPack 5.1.5 / R35.6.2 kernel (Linux 5.10.216-tegra).
- Support configuration and usage of 12-channel YUV cameras.

## 2. Upgrade Package Content Description

- Upgrade script
  - `fg24-12ch.JAO.R35.6.2.sh`
- Driver and DTB
  - `kernel/fzcam.ko` — Camera driver
  - `kernel/dtb/tegra234-p3701-0000-p3737-0000.dtb`
  - `kernel/dtb/tegra234-p3701-0004-p3737-0000.dtb`
  - `kernel/dtb/tegra234-p3701-0005-p3737-0000.dtb`
- Applications and configuration
  - `fzcam_app/usr/local/bin/fzcam_ui` — QT UI application
  - `fzcam_app/usr/local/bin/fzcam_cfg` — Camera/Serdes configuration tool
  - `fzcam_app/etc/fzcam_cfg.ini` — Camera link configuration file
  - `fzcam_app/fzcam_cfg.service` — systemd service

## 3. Execute Upgrade on Jetson

Copy the entire directory to Jetson (choose any method):

```bash
scp -r JetPack5.1.5_R35.6.2_JAO_FG12CH_YUV_Cameras nvidia@<JETSON_IP>:
```

Enter the directory on Jetson and execute the upgrade script (sudo required):

```bash
cd ~/JetPack5.1.5_R35.6.2_JAO_FG12CH_YUV_Cameras
sudo bash fg24-12ch.JAO.R35.6.2.sh
```

What the script does (key points):
- Check if `/etc/nv_tegra_release` is R35.6.2, and exit if it doesn't match
- Install configuration file `/etc/fzcam_cfg.ini`
- Install applications:
  - `/usr/local/bin/fzcam_cfg`
  - `/usr/local/bin/fzcam_ui`
- Install and enable systemd service: `/etc/systemd/system/fzcam_cfg.service`
- Install driver `fzcam.ko` to `/lib/modules/5.10.216-tegra/kernel/drivers/media/i2c/` and run `insmod` / `depmod`
- Backup and replace DTB files (automatically selects the correct SKU dtb based on current hardware)
- Restart after secondary confirmation

## 4. Post-restart Configuration and Image Verification

### 4.1 Run UI Configuration

```bash
sudo fzcam_ui
```

After selecting the manufacturer/model in the UI:
- Click "Save Configuration"
- Then click "Run Configuration"
- Observe if Link status is 1 (indicating the link is locked and has video data)

### 4.2 GStreamer Quick Verification

Taking video0 as an example:

```bash
gst-launch-1.0 v4l2src device=/dev/video0 ! 'video/x-raw,format=UYVY,width=1920,height=1080' ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
```

### 4.3 Device Node Check

```bash
uname -r
# Expected: 5.10.216-tegra

lsmod | grep fzcam
# Expected: fzcam <size> 0

ls -l /dev/video*
# Expected: multiple /dev/videoX nodes
```

## 5. Common Issues

### 5.1 JetPack Version Mismatch

The script will check if `/etc/nv_tegra_release` is R35.6.2, and exit if it doesn't match. Please confirm the current system is JetPack 5.1.5.

### 5.2 System Fails to Boot After DTB Replacement

If the system becomes unbootable after DTB replacement, restore the backup:
```bash
sudo cp /boot/dtb/kernel_tegra234-p3701-0000-p3737-0000.dtb.orig \
        /boot/dtb/kernel_tegra234-p3701-0000-p3737-0000.dtb
sudo reboot
```

### 5.3 No Video Nodes / No Image

Symptoms: No video nodes or no image output.
Solution:
1. Confirm the camera connection is correct
2. Re-execute the upgrade script and restart
3. Check if the driver is loaded: `lsmod | grep fzcam`
