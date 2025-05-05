#!/bin/bash

# Colores
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

check_items() {
    local title="$1"
    local -n files_to_check=$2
    local -n dirs_to_check=$3
    local all_ok=true
    local missing_items=()
    local check_performed=false

    echo -e "${BLUE}--- $title ---${NC}"

    if [ ${#files_to_check[@]} -gt 0 ]; then
        check_performed=true
        echo -e "${BLUE}Checking files:${NC}"
        for item in "${files_to_check[@]}"; do
            local full_path="$item" 
            if [[ -e "$full_path" ]]; then
                echo -e " ${GREEN}✔${NC} Found: $full_path"
            else
                echo -e " ${RED}✘${NC} Missing: $full_path"
                all_ok=false
                missing_items+=("$full_path")
            fi
        done
    fi

    if [ ${#dirs_to_check[@]} -gt 0 ]; then
        check_performed=true
        echo -e "${BLUE}Checking directories:${NC}"
        for item in "${dirs_to_check[@]}"; do
            local full_path="$item" 
            if [[ -d "$full_path" ]]; then
                echo -e " ${GREEN}✔${NC} Found: $full_path"
            else
                echo -e " ${RED}✘${NC} Missing: $full_path"
                all_ok=false
                missing_items+=("$full_path")
            fi
        done
    fi

    # Summary
    echo -e "${BLUE}-------------------------------------------------${NC}"
    if ! $check_performed; then
         echo -e "${YELLOW}ℹ No items specified for checking in '$title'.${NC}"
         return 0 # Nothing to check, technically success
    elif $all_ok; then
        echo -e "${GREEN}✔ All required items for '$title' exist.${NC}"
        return 0 # Success
    else
        echo -e "${RED}✘ Missing items for '$title':${NC}"
        for missing in "${missing_items[@]}"; do
            echo -e "  - ${YELLOW}$missing${NC}"
        done
        return 1 # Failure
    fi
}


kernel_files=(
    "sysdrv/out/bin/board_uclibc_rv1106/Image"
    "sysdrv/out/bin/board_uclibc_rv1106/Image.gz"
    "sysdrv/out/bin/board_uclibc_rv1106/resource.img"
    "sysdrv/out/bin/board_uclibc_rv1106/rv1106g-luckfox-pico-pro.dtb"
    "sysdrv/out/bin/board_uclibc_rv1106/vmlinux"
    "sysdrv/out/bin/board_uclibc_rv1106/zImage"
    "sysdrv/out/image_uclibc_rv1106/boot.img"
)
kernel_dirs=(
    "sysdrv/source/objs_kernel"
)

uboot_files=(
    "sysdrv/out/bin/board_uclibc_rv1106/uboot.debug.tar.bz2"
    "sysdrv/out/image_uclibc_rv1106/download.bin"
    "sysdrv/out/image_uclibc_rv1106/idblock.img"
    "sysdrv/out/image_uclibc_rv1106/uboot.img"
)

buildroot_rootfs_dirs=(
    "sysdrv/out/rootfs_uclibc_rv1106"
    "sysdrv/out/rootfs_uclibc_rv1106/bin"
    "sysdrv/out/rootfs_uclibc_rv1106/dev"
    "sysdrv/out/rootfs_uclibc_rv1106/etc"
    "sysdrv/out/rootfs_uclibc_rv1106/lib"
    "sysdrv/out/rootfs_uclibc_rv1106/media"
    "sysdrv/out/rootfs_uclibc_rv1106/mnt"
    "sysdrv/out/rootfs_uclibc_rv1106/opt"
    "sysdrv/out/rootfs_uclibc_rv1106/proc"
    "sysdrv/out/rootfs_uclibc_rv1106/root"
    "sysdrv/out/rootfs_uclibc_rv1106/run"
    "sysdrv/out/rootfs_uclibc_rv1106/sbin"
    "sysdrv/out/rootfs_uclibc_rv1106/sys"
    "sysdrv/out/rootfs_uclibc_rv1106/tmp"
    "sysdrv/out/rootfs_uclibc_rv1106/usr"
    "sysdrv/out/rootfs_uclibc_rv1106/var"
)
buildroot_rootfs_links=(
    "sysdrv/out/rootfs_uclibc_rv1106/lib32"      # -> lib
    "sysdrv/out/rootfs_uclibc_rv1106/linuxrc"    # -> bin/busybox
)

buildroot_rootfs_prereq_files=(
    "sysdrv/source/buildroot/buildroot-2023.02.6/output/images/rootfs.tar"
)


dosfstools_rootfs_usr_dirs=(
    "sysdrv/out/rootfs_uclibc_rv1106/usr/bin"
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin"
)
dosfstools_rootfs_usr_files=(
    "sysdrv/out/rootfs_uclibc_rv1106/usr/bin/io"
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/fatlabel"
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/fsck.fat"
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/mkfs.fat"
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/dosfsck"     # -> fsck.fat
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/dosfslabel"  # -> fatlabel
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/fsck.msdos"  # -> fsck.fat
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/fsck.vfat"   # -> fsck.fat
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/mkdosfs"     # -> mkfs.fat
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/mkfs.msdos"  # -> mkfs.fat
    "sysdrv/out/rootfs_uclibc_rv1106/usr/sbin/mkfs.vfat"   # -> mkfs.fat
)

# --- Check Functions (Wrappers) ---

check_kernel_files() {
    check_items "Kernel Files & Objects Check" kernel_files kernel_dirs
}

check_uboot_files() {
    check_items "U-Boot Files & Dirs Check" uboot_files uboot_dirs
}

check_buildroot_rootfs_files() {
    check_items "Buildroot Rootfs Prerequisite Check" buildroot_rootfs_prereq_files empty_array empty_array
    local prereq_status=$?
    if [[ $prereq_status -ne 0 ]]; then
        echo -e "${YELLOW}ℹ Prerequisite rootfs.tar missing. Skipping detailed rootfs structure check.${NC}"
        return 1
    fi
    echo ""
    check_items "Buildroot Rootfs Structure Check" empty_array buildroot_rootfs_dirs buildroot_rootfs_links
    return $?
}

check_dosfstools_rootfs_files() {
    check_items "Dosftools Rootfs Files Check" dosfstools_rootfs_usr_files dosfstools_rootfs_usr_dirs dosfstools_rootfs_usr_links
}

# --- Main Execution Logic ---

echo -e "${BLUE}Starting Build Process Checks...${NC}"


check_dosfstools_rootfs_files
if [[ $? -ne 0 ]]; then
    echo -e "${RED}✘ Error: Missing dosfstools files/directories. Aborting build.${NC}"
    exit 1
fi

echo -e "${GREEN}✔ All prerequisite file checks passed successfully!${NC}"
echo ""
