# Jetson AGX Orin Devkits（中文）

本目录汇总适用于 **Jetson AGX Orin Devkits** 的 SerDes 相机/驱动/DTBO 升级包（按 JetPack 版本分类）。

## 1. 快速入口（按 JetPack 版本）

### JetPack 6.2（R36.4.x）

- JAO + FG96-8CH YUV
  - [JetPack_6.2_R36.4_JAO_FG96_8CH_YUV_Cameras/README-CN.md](JetPack_6.2_R36.4.x/JetPack_6.2_R36.4_JAO_FG96_8CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.2_R36.4_JAO_FG96_8CH_YUV_Cameras/README-EN.md](JetPack_6.2_R36.4.x/JetPack_6.2_R36.4_JAO_FG96_8CH_YUV_Cameras/README-EN.md)

### JetPack 6.2.2（R36.5.x）

- JAO + FG96-8CH YUV
  - [JetPack_6.2.2_R36.5_JAO_FG96_8CH_YUV_Cameras/README-CN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_JAO_FG96_8CH_YUV_Cameras/README-CN.md)
  - [JetPack_6.2.2_R36.5_JAO_FG96_8CH_YUV_Cameras/README-EN.md](JetPack_6.2.2_R36.5/JetPack_6.2.2_R36.5_JAO_FG96_8CH_YUV_Cameras/README-EN.md)
- Realsense D4XX + JAO + FG96-8CH
  - [Realsense_D4XX_JAO_FG96_8CH_Driver_JP6.2.2_R36.5/README.zh-CN.md](JetPack_6.2.2_R36.5/Realsense_D4XX_JAO_FG96_8CH_Driver_JP6.2.2_R36.5/README.zh-CN.md)
  - [Realsense_D4XX_JAO_FG96_8CH_Driver_JP6.2.2_R36.5/README.en.md](JetPack_6.2.2_R36.5/Realsense_D4XX_JAO_FG96_8CH_Driver_JP6.2.2_R36.5/README.en.md)
- 335Lg + JAO FG SerDes Driver
  - [335Lg_JAO_FG_SERDES_Driver_JP6.2.2_R36.5/README.md](JetPack_6.2.2_R36.5/335Lg_JAO_FG_SERDES_Driver_JP6.2.2_R36.5/README.md)

## 2. 使用建议

- 先确认 JetPack 版本（`cat /etc/nv_tegra_release`），选择对应 JetPack 目录。
- 每个升级包目录内都有单独 README，按该 README 执行脚本/拷贝文件即可。
