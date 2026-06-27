# FG96 8CH YUV Cameras JetPack 7.2 (R39.2.x) SOP

Directory: JetPack_7.2_R39.2_JAO_FG96_8CH_YUV_Cameras

## 1. Objectives

- Complete firmware/application upgrade on Jetson AGX Orin + FG96 8CH YUV Cameras kit.
- Supports JetPack 7.2 / R39.2.x kernel (Linux 6.8.12-tegra).
- Support configuration and usage of 8-channel YUV cameras.

## 2. Upgrade Package Content Description

- Upgrade script
  - `fg96.8ch.JAO.R39.2.sh`
- DTBO Device Tree Overlay
  - `rootfs/boot/tegra234-p3737-camera-fzcam-fg96-8ch-4lanes.dtbo`
- Drivers and configuration
  - `rootfs/lib/modules/6.8.12-1021-tegra/updates/drivers/media/i2c/fzcam.ko`
- Applications and configuration
  - `fzcam_app/usr/local/bin/fzcam_ui`
  - `fzcam_app/usr/local/bin/fzcam_cfg`
  - `fzcam_app/etc/fzcam_cfg.ini`
  - `fzcam_app/fzcam_cfg.service`
- Demo image
  - `pics_videos/fg96-8ch-fzcam-ui.png`

## 3. Execute Upgrade on Jetson

Copy the entire directory to Jetson (choose any method):

```bash
scp -r JetPack_7.2_R39.2_JAO_FG96_8CH_YUV_Cameras nvidia@<JETSON_IP>:
```

Enter the directory on Jetson and execute the upgrade script (sudo required):

```bash
cd ~/JetPack_7.2_R39.2_JAO_FG96_8CH_YUV_Cameras
sudo bash fg96.8ch.JAO.R39.2.sh
```

What the script does (key points):
- Check if JetPack version is R39.2.0 / R39.2.3
- Install driver `fzcam.ko` and run `insmod` / `depmod`
- Install configuration file `/etc/fzcam_cfg.ini`
- Install applications:
  - `/usr/local/bin/fzcam_cfg`
  - `/usr/local/bin/fzcam_ui`
- Install and enable systemd service: `/etc/systemd/system/fzcam_cfg.service`
- Install DTBO and configure hardware via Jetson-IO: `Jetson Camera FG96_8CH_8xYUV`
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

![8CH FZCAM UI](pics_videos/fg96-8ch-fzcam-ui.png)

### 4.2 GStreamer Quick Verification

Taking video0 as an example:

```bash
gst-launch-1.0 v4l2src device=/dev/video0 ! 'video/x-raw,format=UYVY,width=1920,height=1080' ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
```

### 4.3 Device Node Check

```bash
uname -r
# Expected: 6.8.12-1021-tegra

lsmod | grep fzcam
# Expected: fzcam <size> 0

ls -l /dev/video*
# Expected: multiple /dev/videoX nodes
```

## 5. Common Issues

### 5.1 JetPack Version Mismatch

The script will check if `/etc/nv_tegra_release` is R39.2.0 / R39.2.3, and exit if it doesn't match. Please confirm the current system is JetPack 7.2.

### 5.2 No Video Nodes / No Image

Symptoms: No video nodes or no image output.
Solution:
1. Confirm the camera connection is correct
2. Re-execute the upgrade script and restart
3. Verify DTBO is enabled: `sudo /opt/nvidia/jetson-io/config-by-hardware.py -n 2="Jetson Camera FG96_8CH_8xYUV"`

### 5.3 Kernel Module Path Mismatch

If `fzcam.ko` does not match the current `uname -r`, place the matching `fzcam.ko` into `rootfs/lib/modules/<actual-kernel-version>/updates/drivers/media/i2c/` before running the script.