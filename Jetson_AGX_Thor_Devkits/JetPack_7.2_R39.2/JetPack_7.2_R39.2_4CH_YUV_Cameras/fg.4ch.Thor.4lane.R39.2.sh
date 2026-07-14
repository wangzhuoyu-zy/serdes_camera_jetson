#!/bin/bash
<<COMMENT
#
# 2026-07-08 AGX Thor (P3971) FG-4CH YUV 4-Lane camera firmware install script
#                  JetPack R39.2 / L4T R39.2 / kernel 6.8.12-1021-tegra
#
COMMENT

set -e

clear

red_print() {
    echo -e "\e[1;31m$1\e[0m"
}

green_print() {
    echo -e "\e[1;32m$1\e[0m"
}

# ---------------------------------------------------------------------------
# 1. JetPack / L4T 版本校验
#    当前机器实际版本字符串（来自 /etc/nv_tegra_release）：
#      "# R39 (release), REVISION: 2.0, GCID: 45755727, ..."
#    兼容区间：R39.2.x
# ---------------------------------------------------------------------------
str_ver_thor="# R39 (release), REVISION: 2.0"

red_print "------------AGX Thor FG-4CH YUV 4-Lane Firmware Upgrade 固件升级----------------------"
echo ''

str=$(cat /etc/nv_tegra_release)
if [[ $str == *$str_ver_thor* ]]; then
    echo $str_ver_thor
    green_print 'JetPack R39.2 is correct, JetPack版本匹配自动升级'
else
    red_print 'JetPack is not correct, please check and run again. JetPack版本不匹配,检查后再运行'
    exit 1
fi

green_print 'Press Enter to continue'
read key
echo ''

# ---------------------------------------------------------------------------
# 2. 内核模块 fzcam.ko 安装/加载
#    注意：
#      - 目标机器是 aarch64 + 6.8.12-1021-tegra；当前 ko 已就放在
#        rootfs/lib/modules/6.8.12-1021-tegra/updates/drivers/media/i2c/fzcam.ko
#      - 当前 lsmod 已显示 fzcam 已加载，仍保留安装逻辑以便刷新
# ---------------------------------------------------------------------------
KVER=$(uname -r)
KO_SRC=rootfs/lib/modules/${KVER}/updates/drivers/media/i2c/fzcam.ko
KO_DST=/lib/modules/${KVER}/updates/drivers/media/i2c/fzcam.ko

if [[ ! -f "$KO_SRC" ]]; then
    red_print "fzcam.ko not found at $KO_SRC, abort."
    exit 1
fi

sudo cp "$KO_SRC" "$KO_DST"
sudo depmod -a

if ! lsmod | grep -q '^fzcam[[:space:]]'; then
    sudo modprobe fzcam || sudo insmod "$KO_DST"
fi

# ---------------------------------------------------------------------------
# 3. 用户态程序 / 配置文件
#    当前包内结构：
#      fzcam_app/etc/fzcam_cfg.ini
#      fzcam_app/usr/local/bin/fzcam_cfg
#      fzcam_app/usr/local/bin/fzcam_ui
#      fzcam_app/fzcam_cfg.service
# ---------------------------------------------------------------------------
sudo install -m 0644 fzcam_app/etc/fzcam_cfg.ini     /etc/fzcam_cfg.ini
sudo install -m 0755 fzcam_app/usr/local/bin/fzcam_cfg /usr/local/bin/fzcam_cfg
sudo install -m 0755 fzcam_app/usr/local/bin/fzcam_ui  /usr/local/bin/fzcam_ui

# ---------------------------------------------------------------------------
# 4. systemd 服务
# ---------------------------------------------------------------------------
sudo install -m 0644 fzcam_app/fzcam_cfg.service /etc/systemd/system/fzcam_cfg.service
sudo systemctl daemon-reload
sudo systemctl enable fzcam_cfg.service
# 不在脚本里直接 start，避免 reboot 前服务提前拉起造成状态混乱；
# 如需立刻启动，请手动执行：sudo systemctl start fzcam_cfg.service

# ---------------------------------------------------------------------------
# 5. 设备树 overlay
#    板级：AGX Thor   载板 P3971-0000 + P3834-xxxx  (p4071-0000+p3834-0008)
#    当前包内提供：
#      rootfs/boot/tegra264-p3971-camera-fg-4ch-overlay.dtbo
#    overlay-name = "Jetson Camera Thor_FG_4CH"   (与 jetson-io 注册名一致)
#
#    原 Orin 脚本里用的是：
#      tegra234-p3767-camera-p3768-serdes-4ch-4lanes.dtbo            ← 已不存在
#      /opt/nvidia/jetson-io/config-by-hardware.py -n 2="Camera FG-4CH-4Lanes-YUV"
#    Thor 上 overlay-name 改成 "Jetson Camera Thor_FG_4CH" 才能匹配 dtbo，
#    header 索引在 Thor 上是 1（不是 Orin 的 2）。
# ---------------------------------------------------------------------------
DTBO_SRC=rootfs/boot/tegra264-p3971-camera-fg-4ch-overlay.dtbo
DTBO_DST=/boot/tegra264-p3971-camera-fg-4ch-overlay.dtbo

if [[ ! -f "$DTBO_SRC" ]]; then
    red_print "DTBO not found at $DTBO_SRC, abort."
    exit 1
fi

sudo cp "$DTBO_SRC" "$DTBO_DST"

# jetson-io 必须用 sudo；无 TTY 时也能跑
sudo /opt/nvidia/jetson-io/config-by-hardware.py -n 1="Jetson Camera Thor_FG_4CH" || {
    red_print 'jetson-io config-by-hardware.py failed, 请手动检查 /opt/nvidia/jetson-io/Headers 配置'
    exit 1
}

green_print 'Please press any key to reboot Jetson, after setting dtbo'
read key

sudo reboot