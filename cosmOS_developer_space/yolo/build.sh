#!/bin/bash

# Build script for YOLO using CMake
# Author: felix@cosmOS

set -e

ROOT_PWD=$(cd "$(dirname $0)" && cd -P "$(dirname "$SOURCE")" && pwd)
BUILD_TYPE=${1:-Release}
BUILD_DIR="build"
INSTALL_DIR="${ROOT_PWD}/install"

echo "Building YOLO with CMake..."
echo "Build type: $BUILD_TYPE"
echo "Install dir: $INSTALL_DIR"

if [ "$1" = "clean" ]; then
    if [ -d "${ROOT_PWD}/build" ]; then
        rm -rf "${ROOT_PWD}/build"
        echo "${ROOT_PWD}/build has been deleted!"
    fi
    if [ -d "${ROOT_PWD}/install" ]; then
        rm -rf "${ROOT_PWD}/install"
        echo "${ROOT_PWD}/install has been deleted!"
    fi
    exit
fi

# Clean previous build
if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
    echo "Cleaned previous build"
fi

# Clean previous install
if [ -d "install" ]; then
    rm -rf "install"
    echo "Cleaned previous install"
fi

src_dir="example/luckfox_pico_rtsp_yolov5"
if [[ -d "$src_dir" ]]; then
    mkdir ${ROOT_PWD}/build
    cd ${ROOT_PWD}/build
    
    cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
          -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
          -DCMAKE_C_FLAGS_RELEASE="-Os -DNDEBUG" \
          -DCMAKE_CXX_FLAGS_RELEASE="-Os -DNDEBUG" \
          -DEXAMPLE_DIR="$src_dir" \
          -DEXAMPLE_NAME="luckfox_pico_rtsp_yolov5" \
          -DLIBC_TYPE="uclibc" \
          ..
    
    make -j$(nproc)
    make install
    
    echo "YOLO built successfully!"
    echo "Executable location: $INSTALL_DIR/uclibc/luckfox_pico_rtsp_yolov5_demo/luckfox_pico_rtsp_yolov5"
    
    if [ -f "$INSTALL_DIR/uclibc/luckfox_pico_rtsp_yolov5_demo/luckfox_pico_rtsp_yolov5" ]; then
        echo ""
        echo "Binary information:"
        ls -la "$INSTALL_DIR/uclibc/luckfox_pico_rtsp_yolov5_demo/luckfox_pico_rtsp_yolov5"
        file "$INSTALL_DIR/uclibc/luckfox_pico_rtsp_yolov5_demo/luckfox_pico_rtsp_yolov5"
    else
        echo "Checking build directory for executable..."
        find . -name "luckfox_pico_rtsp_yolov5" -type f -executable
    fi
else
    echo "Error: Directory $src_dir does not exist!"
    exit 1
fi