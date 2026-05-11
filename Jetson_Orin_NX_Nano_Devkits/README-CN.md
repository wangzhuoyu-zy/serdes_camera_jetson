# Jetson Orin NX / Nano Devkits（中文）

本目录汇总适用于 **Jetson Orin NX / Orin Nano Devkits** 的 SerDes 相机驱动/DTBO/应用升级包（按 JetPack 版本分类）。

## 1. 快速入口（按 JetPack 版本）

### JetPack 6.2（R36.4.x）

- 2CH YUV（FG96-2CH）
  - [JetPack_6.2_2CH_YUV_Cameras/README-CN.md](JetPack_6.2_R36.4.x/JetPack_6.2_2CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.2_2CH_YUV_Cameras/README-EN.md](JetPack_6.2_R36.4.x/JetPack_6.2_2CH_YUV_Cameras/README-EN.md)
- 4CH YUV（2lane/4lane）
  - [JetPack_6.1_6.2_6.2.1_4CH_YUV_Cameras/README-CN.md](JetPack_6.2_R36.4.x/JetPack_6.1_6.2_6.2.1_4CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.1_6.2_6.2.1_4CH_YUV_Cameras/README-EN.md](JetPack_6.2_R36.4.x/JetPack_6.1_6.2_6.2.1_4CH_YUV_Cameras/README-EN.md)

### JetPack 6.2.2（R36.5.x）

- 2CH YUV（FG96-2CH）
  - [JetPack_6.2.2_R36.5_2CH_YUV_Cameras/README-CN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_2CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.2.2_R36.5_2CH_YUV_Cameras/README-EN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_2CH_YUV_Cameras/README-EN.md)
- 4CH YUV（FG12-4CH/FG24-4CH）
  - [JetPack_6.2.2_R36.5_4CH_YUV_Cameras/README-CN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_4CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.2.2_R36.5_4CH_YUV_Cameras/README-EN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_4CH_YUV_Cameras/README-EN.md)

## 2. 目录内升级包一般包含什么

- 升级脚本：通常命名为 `fg.*.sh`
- 驱动：`rootfs/lib/modules/<kernel>/updates/drivers/media/i2c/*.ko`
- DTBO：`rootfs/boot/*.dtbo`
- 应用与配置：`fzcam_app/`（包含 `fzcam_ui`、`fzcam_cfg.*lane`、`fzcam_cfg.ini`、systemd 服务等）

## 3. 使用建议

- 先确认 JetPack 版本（`cat /etc/nv_tegra_release`），选择对应目录下的升级包。
- 2lane/4lane 请选择与你的 CSI 排线接法一致的脚本（README 内会说明 CAM0/CAM1 对应关系）。
