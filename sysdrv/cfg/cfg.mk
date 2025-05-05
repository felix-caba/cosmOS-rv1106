CONFIG_SYSDRV_CHIP:=rv1106
CONFIG_SYSDRV_CROSS := arm-rockchip830-linux-uclibcgnueabihf
TINY_ROOTFS_BUSYBOX_CFG := config_tiny_arm
CONFIG_SYSDRV_PARTITION=32K(env),512K@32K(idblock),256K(uboot),32M(boot),512M(oem),256M(userdata),6G(rootfs)
