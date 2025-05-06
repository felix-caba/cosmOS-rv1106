#!/bin/bash

# Colores
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color


export RK_APP_TYPE=RKIPC_RV1106


export RK_APP_CROSS="arm-rockchip830-linux-uclibcgnueabihf"

PROJECT_TOP_DIR=$(pwd)
ROOT=${PROJECT_TOP_DIR}

export SDK_APP_DIR="${ROOT}/project/app"
export SDK_MEDIA_DIR="${ROOT}/media"

##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS
export RK_APP_OUTPUT="${SDK_APP_DIR}/output"

export RK_PROJECT_OUTPUT="${ROOT}/output/out"
export RK_PROJECT_PATH_APP="${RK_PROJECT_OUTPUT}/app_out"
export RK_PROJECT_PATH_MEDIA="${RK_PROJECT_OUTPUT}/media_out"

##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS

export RK_PROJECT_PACKAGE_ROOTFS_DIR=${RK_PROJECT_OUTPUT}/rootfs_uclibc_rv1106

export RK_PROJECT_OUTPUT_IMAGE="/output/image"
export RK_PARTITION_FS_TYPE_CFG="rootfs@IGNORE@ext4,userdata@/userdata@ext4,oem@/oem@ext4"
export RK_PARTITION_CMD_IN_ENV="32K(env),512K@32K(idblock),256K(uboot),32M(boot),512M(oem),256M(userdata),6G(rootfs)"

export RK_JOBS=3

function msg_info() {
    echo -e "${GREEN}Info: $1${NC}"
}

function msg_error() {
    echo -e "${RED}Error: $1${NC}"
}

function finish_build() {
    msg_info "Running ${FUNCNAME[1]} succeeded."
    cd $PROJECT_TOP_DIR
}


function build_media() {
    echo "============Start building media============"
    make -C ${SDK_MEDIA_DIR}
    finish_build
}

function clean_media() {
    echo "============Start cleaning media============"
    make distclean -C ${SDK_MEDIA_DIR}
    rm -rf $RK_PROJECT_PATH_MEDIA
    finish_build
}

function build_app() {

    export RK_APP_CHIP="rv1106"
    echo "============Start building app============"
    echo "=====If theres no media built, fails======"
    echo "=========================================="

    if [ ! -d ${RK_PROJECT_PATH_MEDIA} ]; then
        msg_error "Media not built, please build media first."
        exit 1
    fi

    test -d ${SDK_APP_DIR} && make -C ${SDK_APP_DIR}
    finish_build
}


