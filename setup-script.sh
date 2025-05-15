#!/bin/bash
# filepath: /home/felix/docker-shared/cosmOS-rv1106/setup-script.sh

# --- WARNING ---
echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
echo "!!! RUN ONLY AFTER SUBMODULE INIT IS EXECUTED       !!!"
echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
echo ""
read -p "Press Enter to continue if submodules are initialized, or Ctrl+C to cancel..."

# --- Install Dependencies ---
echo "Updating package list and installing dependencies..."
sudo apt update && sudo apt install -y \
    adduser apt autoconf base-files base-passwd bash bc bison bsdutils cmake coreutils cpio dash debconf debianutils device-tree-compiler diffutils dpkg e2fsprogs expect fakeroot file findutils flex g++ g++-multilib gawk gcc gcc-12-base gcc-multilib git gperf gpgv grep gzip hostname init-system-helpers libacl1 libapt-pkg6.0 libattr1 libaudit-common libaudit1 libblkid1 libbz2-1.0 libc-bin libc6 libcap-ng0 libcap2 libcom-err2 libcrypt1 libdb5.3 libdebconfclient0 libext2fs2 libffi8 libgcc-s1 libgcrypt20 libgmp-dev libgmp10 libgnutls30 libgpg-error0 libgssapi-krb5-2 libhogweed6 libidn2-0 libk5crypto3 libkeyutils1 libkrb5-3 libkrb5support0 liblz4-1 liblzma5 libmount1 libmpc-dev libncurses5-dev libncurses6 libncursesw6 libnettle8 libnsl2 libp11-kit0 libpam-modules libpam-modules-bin libpam-runtime libpam0g libpcre2-8-0 libpcre3 libprocps8 libseccomp2 libselinux1 libsemanage-common libsemanage2 libsepol2 libsmartcols1 libss2 libssl-dev libssl3 libstdc++6 libsystemd0 libtasn1-6 libtinfo6 libtirpc-common libtirpc3 libudev1 libunistring2 libuuid1 libxxhash0 libzstd1 login logsave lsb-base make mawk module-assistant mount ncurses-base ncurses-bin openssh-client openssh-server openssl passwd perl-base pkg-config procps python-is-python3 rsync sed sensible-utils ssh sysvinit-utils tar texinfo u-boot-tools ubuntu-keyring unzip usrmerge util-linux vim zlib1g

if [ $? -ne 0 ]; then
    echo "Error installing dependencies. Exiting."
    exit 1
fi

echo "Dependencies installed successfully."
echo ""


echo "Moving kernel files from cosmOS_kernel_patches/..."

PATCH_DIR="cosmOS_kernel_patches"
KERNEL_SRC_DIR="sysdrv/source/kernel"

if [ ! -d "$PATCH_DIR" ]; then
    echo "Error: Directory '$PATCH_DIR' not found. Make sure it exists and contains the necessary files."
    exit 1
fi

declare -A files_to_move=(
    ["$PATCH_DIR/fiq_debugger.c"]="drivers/soc/rockchip/fiq_debugger/fiq_debugger.c"
    ["$PATCH_DIR/rv1106_defconfig"]="arch/arm/configs/rv1106_defconfig"
    ["$PATCH_DIR/rv1106g-luckfox-pico-max.dts"]="arch/arm/boot/dts/rv1106g-luckfox-pico-max.dts"
    ["$PATCH_DIR/rv1106-luckfox-pico-pro-max-ipc.dtsi"]="arch/arm/boot/dts/rv1106-luckfox-pico-pro-max-ipc.dtsi"
)

# Move each file
for src in "${!files_to_move[@]}"; do
    dest="$KERNEL_SRC_DIR/${files_to_move[$src]}"
    if [ -f "$src" ]; then
        echo "Moving $src to $dest..."
        # Ensure destination directory exists
        mkdir -p "$(dirname "$dest")"
        # Move and force overwrite
        mv -f "$src" "$dest"
        if [ $? -ne 0 ]; then echo "Error moving $src. Exiting."
            exit 1
        fi
    else
        echo "Warning: Source file $src not found. Skipping."
    fi
done

echo "Kernel files moved successfully."

# --- Source Toolchain Environment ---
TOOLCHAIN_ENV_SCRIPT="tools/toolchain/arm-rockchip830-linux-uclibcgnueabihf/env_install_toolchain.sh"
echo "Sourcing toolchain environment script: $TOOLCHAIN_ENV_SCRIPT"

if [ -f "$TOOLCHAIN_ENV_SCRIPT" ]; then
    # Ensure the script has execute permissions before sourcing, just in case
    chmod +x "$TOOLCHAIN_ENV_SCRIPT"
    source "$TOOLCHAIN_ENV_SCRIPT"
    if [ $? -ne 0 ]; then
        echo "Error sourcing $TOOLCHAIN_ENV_SCRIPT. Exiting."
        exit 1
    fi
    echo "Toolchain environment sourced successfully."
else
    echo "Error: Toolchain environment script $TOOLCHAIN_ENV_SCRIPT not found. Exiting."
    exit 1
fi

echo ""
echo "Setup complete."