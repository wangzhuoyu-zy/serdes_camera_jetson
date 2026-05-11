# Jetson Orin NX / Nano Devkits (English)

This folder collects SerDes camera packages for **Jetson Orin NX / Orin Nano Devkits**, grouped by JetPack version.

## 1. Quick Start (by JetPack version)

### JetPack 6.2 (R36.4.x)

- 2CH YUV (FG96-2CH)
  - [JetPack_6.2_2CH_YUV_Cameras/README-CN.md](JetPack_6.2_R36.4.x/JetPack_6.2_2CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.2_2CH_YUV_Cameras/README-EN.md](JetPack_6.2_R36.4.x/JetPack_6.2_2CH_YUV_Cameras/README-EN.md)
- 4CH YUV (2lane/4lane)
  - [JetPack_6.1_6.2_6.2.1_4CH_YUV_Cameras/README-CN.md](JetPack_6.2_R36.4.x/JetPack_6.1_6.2_6.2.1_4CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.1_6.2_6.2.1_4CH_YUV_Cameras/README-EN.md](JetPack_6.2_R36.4.x/JetPack_6.1_6.2_6.2.1_4CH_YUV_Cameras/README-EN.md)

### JetPack 6.2.2 (R36.5.x)

- 2CH YUV (FG96-2CH)
  - [JetPack_6.2.2_R36.5_2CH_YUV_Cameras/README-CN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_2CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.2.2_R36.5_2CH_YUV_Cameras/README-EN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_2CH_YUV_Cameras/README-EN.md)
- 4CH YUV (FG12-4CH/FG24-4CH)
  - [JetPack_6.2.2_R36.5_4CH_YUV_Cameras/README-CN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_4CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.2.2_R36.5_4CH_YUV_Cameras/README-EN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_4CH_YUV_Cameras/README-EN.md)

## 2. What a package typically contains

- Upgrade scripts: usually named `fg.*.sh`
- Kernel modules: `rootfs/lib/modules/<kernel>/updates/drivers/media/i2c/*.ko`
- DTBO overlays: `rootfs/boot/*.dtbo`
- App & config: `fzcam_app/` (includes `fzcam_ui`, `fzcam_cfg.*lane`, `fzcam_cfg.ini`, systemd service, etc.)

## 3. Notes

- Verify JetPack version first: `cat /etc/nv_tegra_release`, then pick the matching package.
- Choose the correct 2lane/4lane script based on your CSI connection (CAM0/CAM1 mapping is described in each package README).
