#!/bin/bash
<<COMMENT
#
# 2024-06-10 AGX Orin JetPack5.x 固件升级脚本V1.0_方竹科技
#
COMMENT

clear
str_ver="# R35 (release), REVISION: 6.2"

red_print(){
	echo -e "\e[1;31m$1\e[0m"
}
green_print(){
	echo -e "\e[1;32m$1\e[0m"
}

red_print "------------FG12CH AGX Orin Devkit 固件升级Firmware Upgrade----------------------"
echo ''
echo $PWD

str=$(cat /etc/nv_tegra_release)
if [[ $str == *$str_ver* ]]; then
	echo $str_ver
	green_print 'JetPack is corret, JetPack版本匹配继续'
else
	echo $str_ver
	red_print 'JetPack is not corret check and run again, JetPack版本不匹配，检查后再运行'
	exit
fi

green_print 'Press Enter to continue'
read key

sudo cp fzcam_app/etc/fzcam_cfg.ini /etc/
sudo cp fzcam_app/usr/local/bin/fzcam_cfg /usr/local/bin/
sudo cp fzcam_app/usr/local/bin/fzcam_ui /usr/local/bin/
sudo chmod +x /usr/local/bin/fzcam_ui
sudo chmod +x /usr/local/bin/fzcam_cfg

sudo cp fzcam_app/fzcam_cfg.service /etc/systemd/system/
sudo chmod 644 /etc/systemd/system/fzcam_cfg.service 
sudo systemctl enable  /etc/systemd/system/fzcam_cfg.service 
sudo systemctl daemon-reload

echo '' 
sudo cp $PWD/kernel/fzcam.ko /lib/modules/5.10.216-tegra/kernel/drivers/media/i2c/
sudo insmod /lib/modules/5.10.216-tegra/kernel/drivers/media/i2c/fzcam.ko
sudo depmod

if [ -e /boot/dtb/kernel_tegra234-p3701-0000-p3737-0000.dtb ];then
	if [ ! -f /boot/dtb/kernel_tegra234-p3701-0000-p3737-0000.dtb.orig ];then
		echo "bakckup dtb"
		sudo cp /boot/dtb/kernel_tegra234-p3701-0000-p3737-0000.dtb /boot/dtb/kernel_tegra234-p3701-0000-p3737-0000.dtb.orig
	fi
	sudo cp $PWD/kernel/dtb/tegra234-p3701-0000-p3737-0000.dtb /boot/dtb/kernel_tegra234-p3701-0000-p3737-0000.dtb
fi

if [ -e /boot/dtb/kernel_tegra234-p3701-0005-p3737-0000.dtb ];then
	if [ ! -f /boot/dtb/kernel_tegra234-p3701-0005-p3737-0000.dtb.orig ];then
		echo "bakckup dtb"
		sudo cp /boot/dtb/kernel_tegra234-p3701-0005-p3737-0000.dtb /boot/dtb/kernel_tegra234-p3701-0005-p3737-0000.dtb.orig
	fi
	sudo cp $PWD/kernel/dtb/tegra234-p3701-0005-p3737-0000.dtb /boot/dtb/kernel_tegra234-p3701-0005-p3737-0000.dtb
fi

green_print 'Upgrade FW success, please press Enter to reboot Jetson Orin'
read key

sudo reboot

