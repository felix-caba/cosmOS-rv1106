SYSDRV_DIR=$(shell pwd)

ROOT_DIR := $(SYSDRV_DIR)/..

BUILDROOT_DIR=/buildroot
BUILDROOT_VER ?= buildroot-2023.02.6
HCITOOL_TOOL_PATH :=$(SYSDRV_DIR)/tools/board/buildroot/hcitool_patch
MPV_PATCH_PATH :=$(SYSDRV_DIR)/tools/board/buildroot/mpv_patch

# Compiler # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 
ARCH = arm
CONFIG_SYSDRV_CROSS := arm-rockchip830-linux-uclibcgnueabihf
CROSS_COMPILE := $(CONFIG_SYSDRV_CROSS)-
# Compiler # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # # 

BUILDROOT_DEFCONFIG := $(SYSDRV_DIR)/tools/board/buildroot/luckfox_pico_defconfig

export LC_ALL=C
SHELL:=/bin/bash
MAKE=make
AT?=@

# LIBC
SYSDRV_LIB_TYPE := uclibc
# LIBC

OUTPUT_ROOTFS_SOURCE_DIR := $(SDK_ROOT_DIR)/output/out/sysdrv_out
OUTPUT_ROOTFS_TOOLS_DIR := $(SDK_ROOT_DIR)/output/out/sysdrv_out


SYSDRV_BUILD_RECOVERY := y
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
############################################################

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

#UBOOT############################################################

#KERNEL############################################################
KERNEL_CFG := rv1106_defconfig
KERNEL_CFG_FRAGMENT :=

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

ifneq ($(OUTPUT_SYSDRV_RAMDISK_DIR),)
SYSDRV_RAMDISK_DIR := $(OUTPUT_SYSDRV_RAMDISK_DIR)
else
SYSDRV_KERNEL_INSTALL_BOOTIMG := y
SYSDRV_KERNEL_UPDATE_DTB := y
endif

#KERNEL############################################################

TOOLCHAIN_RUNTIME_LIB:=runtime_lib
TOOLCHAIN_RUNTIME_LIB_C:=lib.tar.bz2

# OUTS
export OUT_BOARD:=board_$(SYSDRV_LIB_TYPE)_$(CHIP)
export OUT_IMAGE:=image_$(SYSDRV_LIB_TYPE)_$(CHIP)
export OUT_ROOTFS:=rootfs_$(SYSDRV_LIB_TYPE)_$(CHIP)

SYSDRV_DIR_OUT_IMAGE := $(SYSDRV_DIR)/out/$(OUT_IMAGE)
SYSDRV_DIR_OUT_BOARD := $(SYSDRV_DIR)/out/bin/$(OUT_BOARD)
SYSDRV_DIR_OUT_PC := $(SYSDRV_DIR)/out/bin/pc
SYSDRV_DIR_OUT_ROOTFS:=$(SYSDRV_DIR)/out/$(OUT_ROOTFS)
# OUTS

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
	$(call MAROC_COPY_PKG_TO_SYSDRV_OUTPUT, $(OUTPUT_BOARD_BIN_PATH), \
		$(SYSDRV_DIR_OUT_BOARD)/$(KERNEL_DTS:dts=dtb) $(SYSDRV_DIR_OUT_BOARD)/vmlinux)

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
	pushd $(SYSDRV_DIR_OUT_ROOTFS)/lib/ ; mv -fv \
		libasan_preinit.o \
		libasan.so* \
		libtsan.so* \
		libubsan.so* \
		libSegFault.so \
		libmemusage.so* \
		libpcprofile.so* \
		libthread_db*.so* \
		libBrokenLocale* \
		libinproctrace.so \
		$(SYSDRV_DIR_OUT_BOARD); \
	popd

##########################################################################################
#   Build buildroot for CosmOS Kernel
#   Author : Felix
#	Date: 2025
##########################################################################################

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

	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) $(BUILDROOT_DEFCONFIG) -C $(BUILDROOT_DIR)/$(BUILDROOT_VER) 
	$(SYSDRV_DIR)/tools/board/mirror_select/buildroot_mirror_select.sh $(BUILDROOT_DIR)/$(BUILDROOT_VER)/.config
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
rootfs: $(ROOTFS_BUILD_ENV)
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

rootfs_ubi:
	@echo -e "$(C_GREEN) ==sysdrv== build ubi rootfs  $(C_NORMAL)"
	# build rootfs as follow:
	# the pagesize = 2k, blocksize = 128k, part_size = 32M
	# the pagesize = 2k, blocksize = 256k, part_size = 32M
	# the pagesize = 4k, blocksize = 256k, part_size = 32M
	# command format:
	# $(UBI_TOOL) <source dir> <output dir> <partition size> <partition name> <fs type> <fs comp>"
	$(SYSDRV_DIR_OUT_PC)/$(UBI_TOOL) $(SYSDRV_DIR_OUT_ROOTFS) $(SYSDRV_DIR_OUT_IMAGE) $(ROOTFS_PART_SIZE) rootfs $(SYSDRV_ROOTFS_FS_TYPE) $(SYSDRV_ROOTFS_FS_COMP)

rootfs_jffs2:
	@echo -e "$(C_GREEN) ==sysdrv== build jffs2 rootfs  $(C_NORMAL)"
	# use for spi nor
	$(SYSDRV_DIR_OUT_PC)/$(JFFS2_TOOL) $(SYSDRV_DIR_OUT_ROOTFS) $(SYSDRV_ROOTFS_OUT_IMAGE) $(ROOTFS_PART_SIZE)

rootfs_squashfs:
	@echo -e "$(C_GREEN) ==sysdrv== build squashfs rootfs  $(C_NORMAL)"
	# SYSDRV_ROOTFS_FS_COMP support lz4|lzo|lzma|xz|gzip (default gzip)
	$(SYSDRV_DIR_OUT_PC)/$(SQUASHFS_TOOL) $(SYSDRV_DIR_OUT_ROOTFS) $(SYSDRV_ROOTFS_OUT_IMAGE) $(SYSDRV_ROOTFS_FS_COMP)

rootfs_erofs:
	@echo -e "$(C_GREEN) ==sysdrv== build erofs rootfs  $(C_NORMAL)"
	# SYSDRV_ROOTFS_FS_COMP support lz4|lz4hc (default lz4hc)
	$(SYSDRV_DIR_OUT_PC)/$(EROFS_TOOL) $(SYSDRV_DIR_OUT_ROOTFS) $(SYSDRV_ROOTFS_OUT_IMAGE) $(SYSDRV_ROOTFS_FS_COMP)

rootfs_ext4:
	@echo -e "$(C_GREEN) ==sysdrv== build ext4 rootfs  $(C_NORMAL)"
	$(SYSDRV_DIR_OUT_PC)/$(EXT4_TOOL) $(SYSDRV_DIR_OUT_ROOTFS) $(SYSDRV_ROOTFS_OUT_IMAGE) $(ROOTFS_PART_SIZE)

rootfs_initramfs:
	@echo -e "$(C_GREEN) ==sysdrv== build initramfs rootfs  $(C_NORMAL)"
	$(SYSDRV_DIR_OUT_PC)/$(INITRAMFS_TOOL) $(SYSDRV_DIR_OUT_ROOTFS) $(SYSDRV_ROOTFS_OUT_IMAGE)

rootfs_clean:
	$(AT)rm $(SYSDRV_DIR_OUT_ROOTFS)/ -rf
	test ! -d $(BUILDROOT_DIR)/$(BUILDROOT_VER)/output/target  || (\
		rm -rf $(BUILDROOT_DIR)/$(BUILDROOT_VER)/output/target ;\
		find $(BUILDROOT_DIR)/$(BUILDROOT_VER) -name ".stamp_target_installed" -exec rm {} \; ;\
	);
