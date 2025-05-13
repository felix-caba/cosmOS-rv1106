#!/bin/bash
set -e

DEV="/dev/sd8"

if [ "$DEV" == "/dev/sd8" ]; then
    echo "ERROR: Edita el script y reemplaza /dev/sdX con el dispositivo correcto (ej: /dev/sdcard)"
    exit 1
fi

env="output/image/env.img"
boot="output/image/boot.img"
idblock="output/image/idblock.img"
uboot="output/image/uboot.img"
oem="output/image/oem.img"
userdata="output/image/userdata.img"
rootfs="output/image/rootfs.img"

for $file in $env $boot $idblock $uboot $oem $userdata $rootfs; do
    if [ ! -f "$file" ]; then
        echo "ERROR: No se encontró el archivo $file"
        exit 1
    fi
done

echo "Flasheando en $DEV..."

# env.img @ 0x0 (sector 0), 0x40 sectores = 32 KiB
dd if=$env of=$DEV bs=512 seek=$((0x0)) conv=notrunc

# idblock.img @ 0x40 (sector 64), 0x178 sectores = 190 KiB
dd if=$idblock of=$DEV bs=512 seek=$((0x40)) conv=notrunc

# uboot.img @ 0x440 (sector 1088), 0x200 sectores = 256 KiB
dd if=$uboot of=$DEV bs=512 seek=$((0x440)) conv=notrunc

# boot.img @ 0x640 (sector 1600), 0x199B sectores = ~3.2 MiB
dd if=$boot of=$DEV bs=512 seek=$((0x640)) conv=notrunc

# oem.img @ 0x10640 (sector 67136), 0x135B0 sectores = ~104 MiB
dd if=$oem of=$DEV bs=512 seek=$((0x10640)) conv=notrunc

# userdata.img @ 0x110640 (sector 1109824), 0x4C4A sectores = ~24 MiB
dd if=$userdata of=$DEV bs=512 seek=$((0x110640)) conv=notrunc

# rootfs.img @ 0x190640 (sector 1633856), 0x4B390 sectores = ~154 MiB
dd if=$rootfs of=$DEV bs=512 seek=$((0x190640)) conv=notrunc

echo "Flasheo completo en $DEV"
