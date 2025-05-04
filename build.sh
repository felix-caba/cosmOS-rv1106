#!/bin/bash

# Colores
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Función para chequear archivos y directorios del kernel compilado
check_kernel_files() {
    echo -e "${BLUE}--- Chequeando archivos del kernel y rootfs ---${NC}"
    local all_ok=true
    local missing_files=()

    # Lista de archivos y directorios a chequear en /out
    local files_to_check=(
        "sysdrv/out/bin/board_uclibc_rv1106/Image"
        "sysdrv/out/bin/board_uclibc_rv1106/Image.gz"
        "sysdrv/out/bin/board_uclibc_rv1106/resource.img"
        "sysdrv/out/bin/board_uclibc_rv1106/rv1106g-luckfox-pico-pro.dtb"
        "sysdrv/out/bin/board_uclibc_rv1106/vmlinux"
        "sysdrv/out/bin/board_uclibc_rv1106/zImage"
        "sysdrv/out/image_uclibc_rv1106/boot.img"
    )

    # Chequeo de archivos en /out
    for item in "${files_to_check[@]}"; do
        if [[ -e "$item" ]]; then
            echo -e " ${GREEN}✔${NC} Encontrado: $item"
        else
            echo -e " ${RED}✘${NC} Falta:     $item"
            all_ok=false
            missing_files+=("$item")
        fi
    done

    # Chequeo del directorio de objetos del kernel compilado
    local objs_kernel_dir="sysdrv/source/objs_kernel"
    echo -e "${BLUE}--- Chequeando directorio de objetos del kernel ---${NC}"
    if [[ -d "$objs_kernel_dir" ]]; then
        echo -e " ${GREEN}✔${NC} Encontrado: $objs_kernel_dir (Objetos del kernel compilado)"
    else
        echo -e " ${RED}✘${NC} Falta:     $objs_kernel_dir (Objetos del kernel compilado)"
        all_ok=false
        missing_files+=("$objs_kernel_dir")
    fi

    # Resumen final
    echo -e "${BLUE}-------------------------------------------------${NC}"
    if $all_ok; then
        echo -e "${GREEN}✔ Todos los archivos y directorios necesarios existen.${NC}"
        return 0 # Éxito
    else
        echo -e "${RED}✘ Faltan los siguientes archivos/directorios:${NC}"
        for missing in "${missing_files[@]}"; do
            echo -e "  - ${YELLOW}$missing${NC}"
        done
        return 1 
    fi
}

check_kernel_files
if [[ $? -ne 0 ]]; then
    echo -e "${RED}✘ Error: Faltan archivos o directorios necesarios.${NC}"
    exit 1
fi


