# FG24 8CH YUV Cameras JetPack 7.2 (R39.2.x) SOP

Directory: `JetPack_7.2_R39.2_JAO_FG24_8CH_YUV_Cameras`

## 1. Objectives

- Complete firmware/application upgrade on Jetson AGX Orin Devkit + FG24 8CH YUV Cameras kit.
- Supports JetPack 7.2 / R39.2.x kernel (Linux 6.8.12-tegra).
- Support configuration and image capture of 8-channel YUV cameras.

## 2. Upgrade Package Contents

- Upgrade script
  - `fg24.8ch.JAO.R39.2.sh`
- DTBO Device Tree Overlay
  - `rootfs/boot/tegra234-p3737-camera-fzcam-fg24-8ch-4lanes.dtbo`
- Driver and configuration
  - `rootfs/lib/modules/6.8.12-1021-tegra/updates/drivers/media/i2c/fzcam.ko`
- Applications and configuration
  - `fzcam_app/usr/local/bin/fzcam_ui`
  - `fzcam_app/usr/local/bin/fzcam_cfg`
  - `fzcam_app/etc/fzcam_cfg.ini`
  - `fzcam_app/fzcam_cfg.service`
- Reference screenshot
  - `pics_videos/fg96-8ch-fzcam-ui.png` (UI is identical to the FG96 kit; used here only as reference)

## 3. Execute the Upgrade on Jetson

Copy the entire directory to the Jetson (choose any method):

```bash
scp -r JetPack_7.2_R39.2_JAO_FG24_8CH_YUV_Cameras nvidia@<JETSON_IP>:
```

On the Jetson, enter the directory and run the upgrade script (sudo required):

```bash
cd ~/JetPack_7.2_R39.2_JAO_FG24_8CH_YUV_Cameras
sudo bash fg24.8ch.JAO.R39.2.sh
```

What the script does (key steps):

- Verify that the JetPack release is R39.2.0 / R39.2.3
- Install the `fzcam.ko` driver and run `insmod` / `depmod`
- Install the configuration file `/etc/fzcam_cfg.ini`
- Install the applications:
  - `/usr/local/bin/fzcam_cfg`
  - `/usr/local/bin/fzcam_ui`
- Install and enable the systemd service: `/etc/systemd/system/fzcam_cfg.service`
- Install the DTBO and configure hardware via Jetson-IO: `Jetson Camera FG24CH_8xYUV_4Lane`
- Ask for confirmation, then reboot

## 4. Post-reboot Configuration and Image Verification

### 4.1 Run the UI Configuration

```bash
sudo fzcam_ui
```

After selecting the manufacturer/model in the UI:

- Click "Save Configuration"
- Then click "Run Configuration"
- Observe whether the Link status becomes 1 (indicates the link is locked and video data is streaming)

### 4.2 GStreamer Quick Verification

Using `video0` as an example:

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

The script checks whether `/etc/nv_tegra_release` reports R39.2.0 / R39.2.3 and exits if it does not. Please confirm that the current system is running JetPack 7.2.

### 5.2 No Video Nodes / No Image

Symptoms: no video nodes appear, or no image can be captured.

Remedies:

1. Verify the camera connection (all 8 FAKRA links seated, 12V supply present).
2. Re-run the upgrade script and reboot.
3. Verify that the DTBO is enabled:
   ```bash
   sudo /opt/nvidia/jetson-io/config-by-hardware.py -n 2="Jetson Camera FG24CH_8xYUV_4Lane"
   ```

### 5.3 Kernel Module Path Mismatch

If the bundled `fzcam.ko` does not match the running `uname -r`, place the matching `fzcam.ko` into `rootfs/lib/modules/<actual-kernel-version>/updates/drivers/media/i2c/` before running the script.

### 5.4 Confusing FG24 with FG96

The FG24 and FG96 kits share the same `fzcam.ko` driver, but the DTBO and Jetson-IO configuration name are different:

- FG24: `tegra234-p3737-camera-fzcam-fg24-8ch-4lanes.dtbo`, Jetson-IO config `Jetson Camera FG24CH_8xYUV_4Lane`
- FG96: `tegra234-p3737-camera-fzcam-fg96-8ch-4lanes.dtbo`, Jetson-IO config `Jetson Camera FG96_8CH_8xYUV`

Using the wrong DTBO causes link failure or corrupted frames. If a wrong DTBO has been installed, re-run the upgrade script in this directory to restore the correct configuration.