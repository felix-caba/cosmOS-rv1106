# Definir el compilador de Device Tree
DT_COMPILER := dtc

# Definir la arquitectura
ARCH := arm

# Encontrar todos los archivos .dts y generar una lista de archivos .dtb
DT_SOURCES := $(wildcard *.dts)
DTB_FILES := $(DT_SOURCES:.dts=.dtb)

# Declarar objetivos
.PHONY: dtb clean

# Regla por defecto: compilar todos los dtb
dtb: $(DTB_FILES)

# Regla para compilar los archivos .dts a .dtb
$(DTB_FILES): %.dtb: %.dts
	$(DT_COMPILER) -I dts -O dtb -o $@ $<

# Limpiar los archivos generados
clean:
	rm -f $(DTB_FILES)
