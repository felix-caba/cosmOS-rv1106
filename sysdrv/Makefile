C_BLACK  = \e[30;1m
C_RED    = \e[31;1m
C_GREEN  = \e[32;1m
C_YELLOW = \e[33;1m
C_BLUE   = \e[34;1m
C_PURPLE = \e[35;1m
C_CYAN   = \e[36;1m
C_WHITE  = \e[37;1m
C_NORMAL = \033[0m

ROOTFS_BUILD_ENV := rootfs_prepare pctools buildroot boardtools drv

SYSDRV_DIR=$(shell pwd)

ROOT_DIR := $(SYSDRV_DIR)/..

BUILDROOT_DIR := $(SYSDRV_DIR)/source/buildroot
BUILDROOT_VER ?= buildroot-2023.02.6
HCITOOL_TOOL_PATH :=$(SYSDRV_DIR)/tools/board/buildroot/hcitool_patch
MPV_PATCH_PATH :=$(SYSDRV_DIR)/tools/board/buildroot/mpv_patch

# ROOFTS CONFIGURATION # # ROOFTS CONFIGURATION # # ROOFTS CONFIGURATION # # ROOFTS CONFIGURATION #
RK_PROJECT_ROOTFS_TYPE := ext4
SYSDRV_ROOTFS_FS_TYPE := ext4

SYSDRV_JOBS = 4

RK_PARTITION_FS_TYPE_CFG=rootfs@IGNORE@ext4,userdata@/userdata@ext4,oem@/oem@ext4

CONFIG_SYSDRV_PARTITION=32K(env),512K@32K(idblock),256K(uboot),32M(boot),512M(oem),256M(userdata),6G(rootfs)

BOOT_MEDIUM=sd_card

CONFIG_SYSDRV_PARTITION ?=
ROOTFS_PART_SIZE ?= $(shell $(SYSDRV_DIR)/tools/pc/toolkits/get_part_info.sh \
					PART_SIZE "$(CONFIG_SYSDRV_PARTITION)" rootfs)
ROOTFS_PART_NUM ?= $(shell $(SYSDRV_DIR)/tools/pc/toolkits/get_part_info.sh \
				   PART_NUM "$(CONFIG_SYSDRV_PARTITION)" rootfs $(BOOT_MEDIUM))
ENV_PART_SIZE ?= $(shell $(SYSDRV_DIR)/tools/pc/toolkits/get_part_info.sh \
				 PART_SIZE "$(CONFIG_SYSDRV_PARTITION)" env)
ENV_PART_OFFSET ?= $(shell $(SYSDRV_DIR)/tools/pc/toolkits/get_part_info.sh \
				   PART_OFFSET "$(CONFIG_SYSDRV_PARTITION)" env)

ifneq ($(findstring ext4 ext2 jffs2 squashfs ubifs erofs,$(RK_PROJECT_ROOTFS_TYPE)),)
ifneq ($(findstring FAIL,$(ROOTFS_PART_SIZE)),)
$(error ## ROOTFS_PART_SIZE is invalid)
endif
endif
ifneq ($(findstring FAIL,$(ENV_PART_SIZE)),)
$(warning ## ENV_PART_SIZE is invalid)
endif
ifneq ($(findstring FAIL,$(ENV_PART_OFFSET)),)
$(warning ## ENV_PART_OFFSET is invalid)
endif

ifeq ($(BOOT_MEDIUM), sd_card)
SYSDRV_ROOTFS_BUILD_TARGET := rootfs_ext4
ifeq ($(SYSDRV_ROOTFS_FS_TYPE),)
SYSDRV_ROOTFS_FS_TYPE := ext4
endif

KERNEL_CMDLINE_FRAGMENT += rootfstype=$(SYSDRV_ROOTFS_FS_TYPE) \
						   root=/dev/mmcblk1p$(ROOTFS_PART_NUM)
endif

ifeq ($(SYSDRV_ROOTFS_FS_TYPE),squashfs)
SYSDRV_ROOTFS_BUILD_TARGET := rootfs_squashfs
ifneq ($(findstring $(BOOT_MEDIUM),spi_nand slc_nand),)
SYSDRV_ROOTFS_BUILD_TARGET := rootfs_ubi
endif
endif

ifeq ($(SYSDRV_ROOTFS_FS_TYPE),initramfs)
SYSDRV_ROOTFS_BUILD_TARGET := rootfs_initramfs
endif

# ROOFTS CONFIGURATION # # ROOFTS CONFIGURATION # # ROOFTS CONFIGURATION # # ROOFTS CONFIGURATION #

EXT4_TOOL:=mkfs_ext4.sh
UBI_TOOL:=mkfs_ubi.sh
SQUASHFS_TOOL:=mkfs_squashfs.sh
JFFS2_TOOL:=mkfs_jffs2.sh
EROFS_TOOL:=mkfs_erofs.sh
INITRAMFS_TOOL:=mkfs_initramfs.sh

# Compiler # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 

CONFIG_SYSDRV_CROSS := arm-rockchip830-linux-uclibcgnueabihf
CROSS_COMPILE := $(CONFIG_SYSDRV_CROSS)-

ifeq ($(CONFIG_SYSDRV_CROSS),arm-rockchip830-linux-uclibcgnueabihf)
SYSDRV_LIB_TYPE := uclibc
ARCH := arm
SYSDRV_CROSS_CFLAGS += -march=armv7-a -mfpu=neon -mfloat-abi=hard
endif

TOOLCHAIN_FILE := arm-rockchip830-linux-uclibcgnueabihf-gcc
TOOLCHAIN_DIR := $(shell dirname $(shell dirname $(TOOLCHAIN_FILE)))
CHIP := rv1106

export ARCH
export CONFIG_SYSDRV_CROSS
export SYSDRV_CROSS_CFLAGS
export SYSDRV_ARCH_TYPE=$(ARCH)
export CROSS_COMPILE=$(CONFIG_SYSDRV_CROSS)-
export KERNEL_DIR

# Compiler # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 

BUILDROOT_CFG ?= config_normal
BUILDROOT_DEFCONFIG := $(SYSDRV_DIR)/tools/board/buildroot/luckfox_pico_defconfig

export LC_ALL=C
SHELL:=/bin/bash
MAKE=make
AT?=@

OUTPUT_ROOTFS_SOURCE_DIR := $(SDK_ROOT_DIR)/output/out/sysdrv_out
OUTPUT_ROOTFS_TOOLS_DIR := $(SDK_ROOT_DIR)/output/out/sysdrv_out

SYSDRV_BUILD_RECOVERY := n
ifeq ($(SYSDRV_BUILD_RECOVERY),y)
export SYSDRV_KERNEL_OBJS_OUTPUT_DIR := $(SYSDRV_DIR)/source/objs_kernel_recovery
export SYSDRV_KERNEL_UPDATE_DTB := n
endif
export SYSDRV_KERNEL_OBJS_OUTPUT_DIR ?= $(SYSDRV_DIR)/source/objs_kernel

############################################################
define MAROC_COPY_PKG_TO_SYSDRV_OUTPUT
@dir_out=$(shell echo $(1)); \
cpflag=$(shell echo $(3)); \
if [ -n "$$dir_out" ] ;then \
	echo -e "\n\n$(C_GREEN) [INSTALL] $(2) \n    TO \t    $$dir_out $(C_NORMAL)\n\n" ;\
	mkdir -p "$$dir_out"; \
	for item in $(2);do \
		if [ -d "$$item" -a -z "$$cpflag" ]; then cp -raf $$item/* $$dir_out; \
		elif [ -d "$$item" -a "$$cpflag" = "SELF" ]; then cp -arf $$item $$dir_out; \
		elif [ -f "$$item" ]; then cp -af $$item $$dir_out; \
		fi; \
	done; \
fi
endef

define MACRO_CHK_DRIVER_MODULE
@(pkg_src=$(strip $(1)); \
pkg_dst=$(strip $(2)); \
pkg_release=$(strip release_$${pkg_dst}_$(CHIP)_$(ARCH)_asm); \
if [ -d "$$pkg_src/$$pkg_dst" ];then echo "TEST: $$pkg_src/$$pkg_dst" && \
	(rm -f $$KERNEL_DIR/drivers/$$pkg_dst && \
	ln -sf $$pkg_src/$$pkg_dst $$KERNEL_DIR/drivers/$$pkg_dst ); \
	else echo "TEST: $$pkg_src/$$pkg_release" && \
	(rm -f $$KERNEL_DIR/drivers/$$pkg_dst && \
	ln -sf $$pkg_src/$$pkg_release $$KERNEL_DIR/drivers/$$pkg_dst ); \
fi; \
grep -w "$$pkg_dst" $$KERNEL_DIR/drivers/Makefile || \
	(echo "obj-\$$(CONFIG_ROCKCHIP_THUNDER_BOOT)	+= $$pkg_dst/" >> $$KERNEL_DIR/drivers/Makefile ); \
);
endef

define MACRO_CLEAN_DRIVER_MODULE
@(pkg_dst=$(strip $(1)); \
pkg_cmds="sed -i '/\<obj-\$$(CONFIG_ROCKCHIP_THUNDER_BOOT)	+= $(strip $(1))\//d' $$KERNEL_DIR/drivers/Makefile"; \
grep -w "$$pkg_dst" $$KERNEL_DIR/drivers/Makefile && \
eval "$$pkg_cmds";\
);
endef
############################################################

# OUTS
OUT_BOARD:=board_$(SYSDRV_LIB_TYPE)_$(CHIP)
OUT_IMAGE:=image_$(SYSDRV_LIB_TYPE)_$(CHIP)
OUT_ROOTFS:=rootfs_$(SYSDRV_LIB_TYPE)_$(CHIP)

SYSDRV_DIR_OUT_IMAGE := $(SYSDRV_DIR)/out/$(OUT_IMAGE)
SYSDRV_DIR_OUT_BOARD := $(SYSDRV_DIR)/out/bin/$(OUT_BOARD)
SYSDRV_DIR_OUT_PC := $(SYSDRV_DIR)/out/bin/pc
SYSDRV_DIR_OUT_ROOTFS:=$(SYSDRV_DIR)/out/$(OUT_ROOTFS)
# OUTS

################################################################################
export SYSDRV_DIR_OUT_PC
export SYSDRV_DIR_OUT_IMAGE
export SYSDRV_DIR_OUT_BOARD
export SYSDRV_DIR_OUT_ROOTFS
################################################################################

#UBOOT############################################################
RKBIN_DIR := $(SYSDRV_DIR)/source/uboot/rkbin
UBOOT_DIR := $(SYSDRV_DIR)/source/uboot/u-boot

OUTPUT_IMAGE_PATH = $(ROOT_DIR)/output/image

DOWNLOAD_SRC_BIN := *_download_v*.bin
DOWNLOAD_BIN := download.bin
IDBLOCK_IMG := idblock.img
IDBLOCK_SRC_BIN := *_idblock_v*.img
UBOOT_COMPILE_MAKE := $(UBOOT_DIR)/make.sh

UBOOT_CFG = rv1106_defconfig
UBOOT_CFG_FRAGMENT = rk-emmc.config

#UBOOT############################################################

#KERNEL############################################################
KERNEL_CFG := rv1106_defconfig
KERNEL_CFG_FRAGMENT := rv1106-evb.config
KERNEL_DTS := rv1106g-luckfox-pico-pro.dts
KERNEL_DIR := $(SYSDRV_DIR)/source/kernel

KERNEL_DIR_DRV_KO ?= $(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)/drv_ko
KERNEL_DTB :=
ifeq ($(ARCH),arm)
KERNEL_DTB := $(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)/arch/$(ARCH)/boot/dts/$(KERNEL_DTS:dts=dtb)
endif
ifeq ($(ARCH),arm64)
KERNEL_DTB := $(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)/arch/$(ARCH)/boot/dts/rockchip/$(KERNEL_DTS:dts=dtb)
endif

OUTPUT_SYSDRV_RAMDISK_DIR=$(ROOT_DIR)/output/out/ramdisk
SYSDRV_RAMDISK_DIR := $(SYSDRV_DIR_OUT_BOARD)

SYSDRV_KERNEL_INSTALL_BOOTIMG ?= y
SYSDRV_KERNEL_UPDATE_DTB ?= y

#KERNEL############################################################

TOOLCHAIN_RUNTIME_LIB:=runtime_lib
TOOLCHAIN_RUNTIME_LIB_C:=lib.tar.bz2

SYSDRV_KERNEL_MOD_PATH:=$(SYSDRV_DIR)/out/kernel_drv_ko
SYSDRV_ROOTFS_OUT_IMAGE := $(SYSDRV_DIR_OUT_IMAGE)/rootfs_base.img
SYSDRV_UBOOT_ENV_CFG_FILE :=$(SYSDRV_DIR_OUT_IMAGE)/.env.txt

OUTPUT_BOARD_BIN_PATH = $(SYSDRV_DIR)/$(OUT_BOARD)

KERNEL_DTB :=
ifeq ($(ARCH),arm)
KERNEL_DTB := $(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)/arch/$(ARCH)/boot/dts/$(KERNEL_DTS:dts=dtb)
endif

################################################################################
##    set task
################################################################################
all: uboot kernel rootfs env

#clean: uboot_clean kernel_clean buildroot_clean busybox_clean pctools_clean boardtools_clean rootfs_clean drv_clean
clean: uboot_clean kernel_clean busybox_clean pctools_clean boardtools_clean rootfs_clean drv_clean

distclean: clean out_clean

################################################################################
#	prepare
################################################################################
prepare:
	@mkdir -p $(SYSDRV_DIR_OUT_IMAGE)
	@mkdir -p $(SYSDRV_DIR_OUT_BOARD)
	@mkdir -p $(SYSDRV_DIR_OUT_PC)
	@mkdir -p $(SYSDRV_DIR_OUT_ROOTFS)

##########################################################################################
#	build uboot
##########################################################################################
uboot: prepare
	@echo -e "$(C_GREEN) ==sysdrv== build uboot  $(C_NORMAL)"
	@$(MAKE) -C $(UBOOT_DIR) $(UBOOT_CFG) $(UBOOT_CFG_FRAGMENT) || exit 1
	pushd $(UBOOT_DIR);$(UBOOT_COMPILE_MAKE) $(UBOOT_COMPILE_MAKE_OPTS) CROSS_COMPILE=$(CROSS_COMPILE) || exit 1;popd
	$(AT)test ! -f $(UBOOT_DIR)/uboot.img || cp -fv $(UBOOT_DIR)/uboot.img $(SYSDRV_DIR_OUT_IMAGE)
ifeq ($(UBOOT_NAND_PAGE_SIZE),4096)
	$(RKBIN_DIR)/tools/programmer_image_tool -i $(UBOOT_DIR)/$(DOWNLOAD_SRC_BIN) \
		-b $(UBOOT_NAND_BLOCK_SIZE) -p 4 -2 -t spinand -o $(SYSDRV_DIR_OUT_IMAGE) || exit 1
else
	$(AT)cp -fv $(UBOOT_DIR)/$(IDBLOCK_SRC_BIN) $(SYSDRV_DIR_OUT_IMAGE)/$(IDBLOCK_IMG)
endif
	$(AT)cp -fv $(UBOOT_DIR)/$(DOWNLOAD_SRC_BIN) $(SYSDRV_DIR_OUT_IMAGE)/$(DOWNLOAD_BIN)
		$(AT)pushd $(UBOOT_DIR);tar cjf $(SYSDRV_DIR_OUT_BOARD)/uboot.debug.tar.bz2 \
		./spl/u-boot-spl \
		./u-boot \
		./u-boot.map \
		./spl/u-boot-spl.bin \
		./u-boot.bin; popd;
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(OUTPUT_IMAGE_PATH), \
		$(SYSDRV_DIR_OUT_IMAGE)/$(IDBLOCK_IMG) \
		$(SYSDRV_DIR_OUT_IMAGE)/uboot.img \
		$(SYSDRV_DIR_OUT_IMAGE)/$(DOWNLOAD_BIN))
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(OUTPUT_BOARD_BIN_PATH), \
		$(SYSDRV_DIR_OUT_BOARD)/uboot.debug.tar.bz2)

uboot_clean:
	rm -rf $(UBOOT_DIR)/fit
	pushd $(UBOOT_DIR);$(MAKE) CROSS_COMPILE=$(CROSS_COMPILE) distclean >/dev/null;popd



##########################################################################################
#	build kernel
##########################################################################################
kernel_menuconfig:
	cp  $(KERNEL_DIR)/arch/arm/configs/$(KERNEL_CFG)  $(KERNEL_DIR)/.config
	$(MAKE) -C $(KERNEL_DIR) ARCH=$(ARCH) menuconfig
	$(MAKE) -C $(KERNEL_DIR) ARCH=$(ARCH) savedefconfig

kernel_savedefconfig:
	cp  $(KERNEL_DIR)/defconfig $(KERNEL_DIR)/arch/arm/configs/$(KERNEL_CFG)

kernel: prepare

	@echo -e "$(C_GREEN) ==sysdrv== build kernel  $(C_NORMAL)"
	@echo -e "$(C_GREEN) SYSDRV_KERNEL_INSTALL_BOOTIMG = $(SYSDRV_KERNEL_INSTALL_BOOTIMG)  $(C_NORMAL)"

	$(MAKE) -C $(KERNEL_DIR) ARCH=$(ARCH) mrproper

	$(AT)test ! -f $(KERNEL_DIR)/vmlinux || make -C $(KERNEL_DIR) distclean
	@rm -rf $(KERNEL_DTB)
	$(MAKE) O=$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR) -C $(KERNEL_DIR) ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) $(KERNEL_CFG) $(KERNEL_CFG_FRAGMENT) || exit 1
	$(MAKE) O=$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR) -C $(KERNEL_DIR) ARCH=$(ARCH) \
		CROSS_COMPILE=$(CROSS_COMPILE) $(KERNEL_DTS:dts=img) BOOT_ITS=$(KERNEL_DIR)/boot.its -j$(SYSDRV_JOBS) || exit 1

ifeq ($(SYSDRV_KERNEL_INSTALL_BOOTIMG),y)
	cp -fv $(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)/boot.img $(SYSDRV_DIR_OUT_IMAGE)/boot.img
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(OUTPUT_IMAGE_PATH), $(SYSDRV_DIR_OUT_IMAGE)/boot.img)
endif

	# Start install kernel drivers
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(SYSDRV_RAMDISK_DIR), \
		$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)/arch/$(ARCH)/boot/Image* \
		$(KERNEL_DTB) \
		$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)/arch/$(ARCH)/boot/zImage \
		$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)/resource.img)
	cp -fv $(SYSDRV_KERNEL_OBJS_OUTPUT_DIR)/vmlinux $(SYSDRV_DIR_OUT_BOARD)
	cp -fv $(KERNEL_DTB) $(SYSDRV_DIR_OUT_BOARD)

kernel_clean:
	$(MAKE) O=$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR) -C $(KERNEL_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) distclean
	@-rm -rf $(SYSDRV_DIR)/source/objs_*
	@-rm -rf $(SYSDRV_KERNEL_MOD_PATH) $(KERNEL_DIR_DRV_KO)

##########################################################################################
#	prepare rootfs
##########################################################################################
rootfs_prepare: prepare

	@echo -e "$(C_GREEN) ==sysdrv== prepare rootfs   $(C_NORMAL)"
	rm -rf $(SYSDRV_DIR_OUT_ROOTFS)
	mkdir -p $(SYSDRV_DIR_OUT_ROOTFS)
	tar xf $(SYSDRV_DIR)/tools/board/rootfs_script.tar -C $(SYSDRV_DIR_OUT_ROOTFS)
	tar xjf $(TOOLCHAIN_DIR)/$(TOOLCHAIN_RUNTIME_LIB)/$(TOOLCHAIN_RUNTIME_LIB_C) -C $(SYSDRV_DIR_OUT_ROOTFS)
	# Move all files extracted from the toolchain library tarball's lib directory
		@if [ -d "$(SYSDRV_DIR_OUT_ROOTFS)/lib" ]; then \
		echo "Moving files from $(SYSDRV_DIR_OUT_ROOTFS)/lib to $(SYSDRV_DIR_OUT_BOARD)"; \
		mv -fv $(SYSDRV_DIR_OUT_ROOTFS)/lib/* $(SYSDRV_DIR_OUT_BOARD)/ || true; \
		rmdir $(SYSDRV_DIR_OUT_ROOTFS)/lib || true; \
	else \
		echo "Warning: Directory $(SYSDRV_DIR_OUT_ROOTFS)/lib not found after extraction."; \
	fi


##########################################################################################
#   Build buildroot for CosmOS Kernel
#   Author : Felix
#	Date: 2025
##########################################################################################

buildroot_menuconfig:
	@echo -e "$(C_GREEN) ==sysdrv== buildroot memnuconfig   $(C_NORMAL)"
	$(MAKE) $(BUILDROOT_DEFCONFIG) -C $(BUILDROOT_DIR)/$(BUILDROOT_VER)
	$(MAKE) menuconfig -C $(BUILDROOT_DIR)/$(BUILDROOT_VER)
#	$(MAKE) source -C $(BUILDROOT_DIR)/$(BUILDROOT_VER)

buildroot_savedefconfig:
	@echo -e "$(C_GREEN) ==sysdrv== buildroot savedefconfig   $(C_NORMAL)"
	$(MAKE) savedefconfig -C $(BUILDROOT_DIR)/$(BUILDROOT_VER) 

buildroot_create:
	@echo -e "$(C_GREEN) ==sysdrv== buildroot create   $(C_NORMAL)"
	rm $(BUILDROOT_DIR)/$(BUILDROOT_VER) -rf ;\
	mkdir -p $(BUILDROOT_DIR)
	tar xzf $(SYSDRV_DIR)/tools/board/buildroot/$(BUILDROOT_VER).tar.gz -C $(BUILDROOT_DIR)
	cp $(SYSDRV_DIR)/tools/board/buildroot/luckfox_pico_defconfig $(BUILDROOT_DIR)/$(BUILDROOT_VER)/configs/
	cp $(SYSDRV_DIR)/tools/board/buildroot/luckfox_pico_w_defconfig $(BUILDROOT_DIR)/$(BUILDROOT_VER)/configs/
	cp $(SYSDRV_DIR)/tools/board/buildroot/busybox.config $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/busybox/
	cp ${HCITOOL_TOOL_PATH}/0001-Fixed-header-file-errors.patch $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/bluez5_utils/
	cp ${HCITOOL_TOOL_PATH}/0002-Fix-build-errors.patch $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/bluez5_utils/
	cp ${HCITOOL_TOOL_PATH}/0003-fix-compat-wordexp.patch $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/bluez5_utils/
	cp ${MPV_PATCH_PATH}/0002-change-j1.patch $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/mpv/

buildroot: prepare
	@echo -e "$(C_GREEN) ==sysdrv== build buildroot   $(C_NORMAL)"
	test -d $(BUILDROOT_DIR)/$(BUILDROOT_VER) || (\
		mkdir -p $(BUILDROOT_DIR) ;\
		tar xzf $(SYSDRV_DIR)/tools/board/buildroot/$(BUILDROOT_VER).tar.gz -C $(BUILDROOT_DIR) ;\
		cp $(SYSDRV_DIR)/tools/board/buildroot/luckfox_pico_defconfig $(BUILDROOT_DIR)/$(BUILDROOT_VER)/configs/ ;\
		cp $(SYSDRV_DIR)/tools/board/buildroot/luckfox_pico_w_defconfig $(BUILDROOT_DIR)/$(BUILDROOT_VER)/configs/ ;\
		cp $(SYSDRV_DIR)/tools/board/buildroot/busybox.config $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/busybox/ ;\
		cp ${HCITOOL_TOOL_PATH}/0001-Fixed-header-file-errors.patch $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/bluez5_utils/ ;\
		cp ${HCITOOL_TOOL_PATH}/0002-Fix-build-errors.patch $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/bluez5_utils/ ;\
		cp ${HCITOOL_TOOL_PATH}/0003-fix-compat-wordexp.patch $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/bluez5_utils/ ;\
		cp ${MPV_PATCH_PATH}/0002-change-j1.patch $(BUILDROOT_DIR)/$(BUILDROOT_VER)/package/mpv/ ;\
	);

	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $(notdir $(BUILDROOT_DEFCONFIG)) -C $(BUILDROOT_DIR)/$(BUILDROOT_VER) 

	(chmod +x $(SYSDRV_DIR)/tools/board/mirror_select/buildroot_mirror_select.sh; \
	$(SYSDRV_DIR)/tools/board/mirror_select/buildroot_mirror_select.sh $(BUILDROOT_DIR)/$(BUILDROOT_VER)/.config; \
	)

	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) source -C $(BUILDROOT_DIR)/$(BUILDROOT_VER) 
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -j$(SYSDRV_JOBS) -C $(BUILDROOT_DIR)/$(BUILDROOT_VER) 

ifeq ($(SYSDRV_BUILD_RAMDISK_FLAG),YES)
    tar xf $(SYSDRV_DIR)/tools/board/rootfs_script.tar -C $(BUILDROOT_DIR)/$(BUILDROOT_VER)/output/target
	ifneq ($(OUTPUT_SYSDRV_RAMDISK_TINY_ROOTFS_DIR),)
		$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(OUTPUT_SYSDRV_RAMDISK_TINY_ROOTFS_DIR), \
			$(SYSDRV_DIR)/tools/board/buildroot/busybox/init \
			$(BUILDROOT_DIR)/$(BUILDROOT_VER)/output/target)
	else
		$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(SYSDRV_DIR_OUT_ROOTFS), \
			$(SYSDRV_DIR)/tools/board/buildroot/busybox/init \
			$(BUILDROOT_DIR)/$(BUILDROOT_VER)/output/target)
	endif
else
	tar xf $(BUILDROOT_DIR)/$(BUILDROOT_VER)/output/images/rootfs.tar -C $(SYSDRV_DIR_OUT_ROOTFS)
endif

buildroot_clean:
	$(AT)rm -rf $(BUILDROOT_DIR)/$(BUILDROOT_VER)

##########################################################################################
#	build pc tools
##########################################################################################
include $(SYSDRV_DIR)/tools/pc/Makefile.tools.pc.mk
pctools: prepare tools_pc-builds
	@echo -e "$(C_GREEN) ==sysdrv== build tools which run on pc  $(C_NORMAL)"
	-chmod a+x $(SYSDRV_DIR_OUT_PC)/*
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(OUTPUT_ROOTFS_TOOLS_DIR), $(SYSDRV_DIR_OUT_PC), SELF)

pctools_clean:
	@echo -e "$(C_GREEN) ==sysdrv== clean tools which run on pc  $(C_NORMAL)"

##########################################################################################
#	build board tools
##########################################################################################
include $(SYSDRV_DIR)/tools/board/Makefile.tools.board.mk
boardtools: tools_board-builds
	@echo -e "$(C_GREEN) ==sysdrv== build tools which run on board   $(C_NORMAL)"

boardtools_clean: tools_board-clean
	@echo -e "$(C_GREEN) ==sysdrv== build $@ done$(C_NORMAL)"

##########################################################################################
#	build driver ko
##########################################################################################
drv: prepare
	@echo -e "$(C_GREEN) ==sysdrv== build drv $(C_NORMAL)"
	@-rm -rf $(SYSDRV_KERNEL_MOD_PATH) $(KERNEL_DIR_DRV_KO)
	@-mkdir -p $(SYSDRV_KERNEL_MOD_PATH)
	$(MAKE) -C $(KERNEL_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -j$(SYSDRV_JOBS) \
			O=$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR) \
		modules
	$(MAKE) -C $(KERNEL_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -j$(SYSDRV_JOBS) \
		O=$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR) \
		modules_install INSTALL_MOD_STRIP=1 \
		INSTALL_MOD_PATH=$(KERNEL_DIR_DRV_KO)
	$(AT)find $(KERNEL_DIR_DRV_KO)/lib/modules/*/kernel \
		-name "*.ko" -type f -exec cp -fav {} $(SYSDRV_KERNEL_MOD_PATH) \;
	@test ! -d drv_ko || ($(MAKE) O=$(SYSDRV_KERNEL_OBJS_OUTPUT_DIR) -C $(SYSDRV_DIR)/drv_ko KERNEL_DRV_KO_INSTALL_PATH=$(SYSDRV_KERNEL_MOD_PATH))
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(OUTPUT_ROOTFS_SOURCE_DIR), $(SYSDRV_KERNEL_MOD_PATH), SELF)

drv_clean:
	@test ! -d drv_ko || $(MAKE) -C $(SYSDRV_DIR)/drv_ko clean
	@-rm -rf $(SYSDRV_KERNEL_MOD_PATH) $(KERNEL_DIR_DRV_KO)
	@echo -e "$(C_GREEN) ==sysdrv== clean drv $(C_NORMAL)"

##########################################################################################
#	build rootfs
##########################################################################################

ROOTFS_BUILD_ENV_SCRATCH := rootfs_prepare pctools buildroot boardtools drv
ROOT_FS_WO_BUILDROOT := boardtools drv

rootfs: $(ROOTFS_BUILD_ENV_SCRATCH)
	@echo -e "$(C_GREEN) ==sysdrv== build rootfs  $(C_NORMAL)"
	#@if [ "$(TARGET_ROOTFS)" = "custom" ]; then \
	#	test -d $(SYSDRV_DIR)/tools/board/custom_rootfs || ( \
	#		cp -r $(SYSDRV_DIR)/tools/board/custom_rootfs/ $(SYSDRV_DIR)/out/$(OUT_ROOTFS) ; \
	#	); \
	#else \
	#	make -C $(SYSDRV_DIR) strip \
	#fi

	pushd $(SYSDRV_DIR)/out;tar cf $(OUT_ROOTFS).tar $(OUT_ROOTFS);popd
	@# RK_PARTITION_FS_TYPE_CFG is defined outside of sysdrv,
	@# so if defined RK_PARTITION_FS_TYPE_CFG, we ignore SYSDRV_ROOTFS_BUILD_TARGET
	@test "$(ROOTFS_PART_SIZE)" = "FAIL" || \
		test "$(RK_PARTITION_FS_TYPE_CFG)x" != "x" || \
		make -C $(SYSDRV_DIR) $(SYSDRV_ROOTFS_BUILD_TARGET)
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(OUTPUT_ROOTFS_SOURCE_DIR), $(SYSDRV_DIR)/out/$(OUT_ROOTFS).tar )
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(OUTPUT_ROOTFS_TOOLS_DIR), $(SYSDRV_DIR)/out/bin )
	@echo -e "$(C_GREEN) ==sysdrv== finish sysdrv build  $(C_NORMAL)"

strip:
ifeq ($(SYSDRV_BUILD_VERSION),RELEASE)
	@echo -e "$(C_GREEN) ==sysdrv== strip files of rootfs  $(C_NORMAL)"
	$(call MAROC_STRIP_DEBUG_SYMBOL, $(SYSDRV_DIR_OUT_ROOTFS))
endif

rootfs_ext4:
	@echo -e "$(C_GREEN) ==sysdrv== build ext4 rootfs  $(C_NORMAL)"
	$(SYSDRV_DIR_OUT_PC)/$(EXT4_TOOL) $(SYSDRV_DIR_OUT_ROOTFS) $(SYSDRV_ROOTFS_OUT_IMAGE) $(ROOTFS_PART_SIZE)

rootfs_clean:
	$(AT)rm $(SYSDRV_DIR_OUT_ROOTFS)/ -rf
	test ! -d $(BUILDROOT_DIR)/$(BUILDROOT_VER)/output/target  || (\
		rm -rf $(BUILDROOT_DIR)/$(BUILDROOT_VER)/output/target ;\
		find $(BUILDROOT_DIR)/$(BUILDROOT_VER) -name ".stamp_target_installed" -exec rm {} \; ;\
	);

##########################################################################################
#	build env.img
##########################################################################################
env: prepare
	@$(MAKE) -C $(SYSDRV_DIR)/tools/pc/uboot_tools

ifeq ($(BOOT_MEDIUM),sd_card)
	@echo "blkdevparts=mmcblk1:$(CONFIG_SYSDRV_PARTITION)" > $(SYSDRV_UBOOT_ENV_CFG_FILE)
	@test "$(ROOTFS_PART_NUM)" = "FAIL" || \
		echo "sys_bootargs=root=/dev/mmcblk1p$(ROOTFS_PART_NUM)" >> $(SYSDRV_UBOOT_ENV_CFG_FILE)
endif

ifeq ($(findstring FAIL,$(ENV_PART_SIZE)),)
	$(SYSDRV_DIR_OUT_PC)/mkenvimage -s $(ENV_PART_SIZE) -p 0x0 -o $(SYSDRV_DIR_OUT_IMAGE)/env.img $(SYSDRV_UBOOT_ENV_CFG_FILE)
endif

##########################################################################################
#	clean out
##########################################################################################
out_clean:
	@rm $(SYSDRV_DIR)/out -rf