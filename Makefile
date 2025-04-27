# Definir el directorio de Device Tree
DT_DIR := kernel/arch/arm/boot/dts
BUILDROOT_DIR := ./buildroot
BUILDROOT_DEFCONFIG := board/luckfox_pico_defconfig

KERNEL_DEFCONFIG_NAME := rv1106_defconfig
KERNEL_DEFCONFIG := kernel/arch/arm/configs/rv1106_defconfig
KERNEL_CONFIG_PATH := /kernel/arch/arm/configs

# Declarar objetivos
.PHONY: all kernel clean

# Objetivo por defecto
all: kernel

# Configurar Buildroot para usar el defconfig
kernel:
	$(MAKE) -C $(BUILDROOT_DIR) BR2_DEFCONFIG=../$(BUILDROOT_DEFCONFIG) defconfig
	$(MAKE) -C $(BUILDROOT_DIR)

# Limpiar los archivos generados
clean:
	$(MAKE) -C $(BUILDROOT_DIR) clean
