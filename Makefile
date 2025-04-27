# Definir el directorio de Device Tree
DT_DIR := kernel/arch/arm/boot/dts
BUILDROOT_DIR := ./buildroot

# Declarar objetivos
.PHONY: all dtb kernel clean

# Objetivo por defecto
all: dtb kernel

# Llamar al Makefile en el directorio de dts
dtb:
	$(MAKE) -C $(DT_DIR) dtb

kernel:
	$(MAKE) -C $(BUILDROOT_DIR)

# Limpiar los archivos generados
clean:
	$(MAKE) -C $(DT_DIR) clean
	$(MAKE) -C $(DT_DIR) clean
