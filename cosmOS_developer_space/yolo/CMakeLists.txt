cmake_minimum_required(VERSION 3.10)

# Processing Parameters
set(LIBC_TYPE "uclibc")
set(LUCKFOX_SDK_PATH "/home/felix/docker-shared/cosmOS-rv1106/")
set(SDK_PATH "${LUCKFOX_SDK_PATH}")
set(GLIBC_COMPILER $ENV{GLIBC_COMPILER})

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure cross-compilation BEFORE project()
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(${LIBC_TYPE} STREQUAL "uclibc")
    if (NOT SDK_PATH)
        message(WARNING "Please Set Luckfox-pico SDK Path. Such as:export LUCKFOX_SDK_PATH=/home/user/luckfox-pico")
        return()
    else()
        set(TOOLCHAIN_PREFIX "arm-rockchip830-linux-uclibcgnueabihf")
        set(TOOLCHAIN_PATH "${SDK_PATH}/tools/linux/toolchain/${TOOLCHAIN_PREFIX}")
        
        # Verify toolchain exists
        if(NOT EXISTS "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-gcc")
            message(FATAL_ERROR "Toolchain not found at: ${TOOLCHAIN_PATH}")
        endif()
        
        set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-gcc")
        set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-g++")
        set(CMAKE_STRIP "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}-strip")
        
        # Toolchain sysroot for basic build environment
        set(CMAKE_SYSROOT "${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}/sysroot")
        
        # Buildroot sysroot for target runtime libraries (FFmpeg, etc.)
        set(CMAKE_BUILDROOT_SYSROOT "${SDK_PATH}/output/out/rootfs_uclibc_rv1106")
        
        # Buildroot host sysroot for development headers (FFmpeg headers are here!)
        set(CMAKE_BUILDROOT_HOST_SYSROOT "${SDK_PATH}/sysdrv/source/buildroot/buildroot-2023.02.6/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot")
        
        # Set find root paths to search all sysroots
        set(CMAKE_FIND_ROOT_PATH 
            "${CMAKE_SYSROOT}"
            "${CMAKE_BUILDROOT_SYSROOT}"
            "${CMAKE_BUILDROOT_HOST_SYSROOT}")
        
        # Configure find paths
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
        
        # Disable compiler checks for cross-compilation
        set(CMAKE_C_COMPILER_WORKS 1)
        set(CMAKE_CXX_COMPILER_WORKS 1)
    endif()
elseif(${LIBC_TYPE} STREQUAL "glibc")
    if (NOT GLIBC_COMPILER)
        message(WARNING "Please Set arm32 glibc compiler Path. Such as:export GLIBC_COMPILER=/opt/arm-linux-gnueabihf/bin/arm-linux-gnueabihf-")
        return()
    else()
        set(CMAKE_C_COMPILER    "${GLIBC_COMPILER}gcc")
        set(CMAKE_CXX_COMPILER  "${GLIBC_COMPILER}g++")
    endif()
endif()

set(EXAMPLE_NAME "luckfox_pico_rtsp_yolov5")
set(EXAMPLE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/example/${EXAMPLE_NAME}")

if(DEFINED EXAMPLE_DIR)
    message(STATUS "Selected source directory: ${EXAMPLE_DIR}")
else()
    message(FATAL_ERROR "No source directory selected!")
    return()
endif()

project(${EXAMPLE_NAME})

set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
add_definitions(-DRV1106_1103)

# Add global include directories for IntelliSense - include buildroot host sysroot for headers
include_directories(SYSTEM 
    ${CMAKE_BUILDROOT_HOST_SYSROOT}/usr/include
    ${CMAKE_BUILDROOT_SYSROOT}/usr/include
    ${CMAKE_SYSROOT}/usr/include
)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib/${LIBC_TYPE})

# Find FFmpeg headers in buildroot host sysroot
find_path(FFMPEG_INCLUDE_DIR
    NAMES libavcodec/avcodec.h
    PATHS 
        ${CMAKE_BUILDROOT_HOST_SYSROOT}/usr/include
        ${CMAKE_BUILDROOT_SYSROOT}/usr/include
        ${CMAKE_SYSROOT}/usr/include
    NO_DEFAULT_PATH
)

if(FFMPEG_INCLUDE_DIR)
    message(STATUS "FFmpeg headers found at: ${FFMPEG_INCLUDE_DIR}")
    include_directories(${FFMPEG_INCLUDE_DIR})
    set(FFMPEG_FOUND TRUE)
else()
    message(FATAL_ERROR "FFmpeg headers not found")
endif()

# Find FFmpeg libraries in buildroot target sysroot
find_library(AVCODEC_LIBRARY 
    NAMES avcodec
    PATHS 
        ${CMAKE_BUILDROOT_SYSROOT}/usr/lib
        ${CMAKE_BUILDROOT_HOST_SYSROOT}/usr/lib
        ${CMAKE_SYSROOT}/usr/lib
    NO_DEFAULT_PATH
)

find_library(AVFORMAT_LIBRARY 
    NAMES avformat
    PATHS 
        ${CMAKE_BUILDROOT_SYSROOT}/usr/lib
        ${CMAKE_BUILDROOT_HOST_SYSROOT}/usr/lib
        ${CMAKE_SYSROOT}/usr/lib
    NO_DEFAULT_PATH
)

find_library(AVUTIL_LIBRARY 
    NAMES avutil
    PATHS 
        ${CMAKE_BUILDROOT_SYSROOT}/usr/lib
        ${CMAKE_BUILDROOT_HOST_SYSROOT}/usr/lib
        ${CMAKE_SYSROOT}/usr/lib
    NO_DEFAULT_PATH
)

find_library(SWSCALE_LIBRARY 
    NAMES swscale
    PATHS 
        ${CMAKE_BUILDROOT_SYSROOT}/usr/lib
        ${CMAKE_BUILDROOT_HOST_SYSROOT}/usr/lib
        ${CMAKE_SYSROOT}/usr/lib
    NO_DEFAULT_PATH
)

find_library(SWRESAMPLE_LIBRARY 
    NAMES swresample
    PATHS 
        ${CMAKE_BUILDROOT_SYSROOT}/usr/lib
        ${CMAKE_BUILDROOT_HOST_SYSROOT}/usr/lib
        ${CMAKE_SYSROOT}/usr/lib
    NO_DEFAULT_PATH
)

if(AVCODEC_LIBRARY AND AVFORMAT_LIBRARY AND AVUTIL_LIBRARY)
    message(STATUS "FFmpeg libraries found:")
    message(STATUS "  avcodec: ${AVCODEC_LIBRARY}")
    message(STATUS "  avformat: ${AVFORMAT_LIBRARY}")
    message(STATUS "  avutil: ${AVUTIL_LIBRARY}")
    if(SWSCALE_LIBRARY)
        message(STATUS "  swscale: ${SWSCALE_LIBRARY}")
    endif()
    if(SWRESAMPLE_LIBRARY)
        message(STATUS "  swresample: ${SWRESAMPLE_LIBRARY}")
    endif()
else()
    message(WARNING "Some FFmpeg libraries not found")
endif()

# OpenCV 4
set(OpenCV_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/${LIBC_TYPE}/lib/cmake/opencv4")
find_package(OpenCV REQUIRED)

# Thread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(SRC_DIR "${EXAMPLE_DIR}/src")
file(GLOB SRC_FILES "${SRC_DIR}/*.cc")
add_executable(${PROJECT_NAME} ${SRC_FILES})

add_compile_options(-g -Wall
                    -DISP_HW_V30 -DRKPLATFORM=ON -DARCH64=OFF
                    -DROCKIVA -DUAPI2
                    -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 
                    )

set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath-link,${CMAKE_CURRENT_SOURCE_DIR}/lib:/usr/lib")

if(${LIBC_TYPE} STREQUAL "uclibc")
    target_link_libraries(${PROJECT_NAME}
                        ${OpenCV_LIBS}
                        rknnmrt
                        Threads::Threads
                        rockiva
                        sample_comm
                        rockit
                        rockchip_mpp
                        rkaiq
                        pthread
                        rtsp
                        rga
                        )
    if(FFMPEG_FOUND AND AVCODEC_LIBRARY AND AVFORMAT_LIBRARY AND AVUTIL_LIBRARY)
        target_link_libraries(${PROJECT_NAME}
                            ${AVCODEC_LIBRARY}
                            ${AVFORMAT_LIBRARY}
                            ${AVUTIL_LIBRARY}
                            )
        if(SWSCALE_LIBRARY)
            target_link_libraries(${PROJECT_NAME} ${SWSCALE_LIBRARY})
        endif()
        if(SWRESAMPLE_LIBRARY)
            target_link_libraries(${PROJECT_NAME} ${SWRESAMPLE_LIBRARY})
        endif()
    endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE                        
                            ${OpenCV_INCLUDE_DIRS}
                            ${EXAMPLE_DIR}/include
                            ${CMAKE_CURRENT_SOURCE_DIR}
                            ${CMAKE_CURRENT_SOURCE_DIR}/utils
                            ${CMAKE_CURRENT_SOURCE_DIR}/common 
                            ${CMAKE_CURRENT_SOURCE_DIR}/common/isp3.x   
                            ${CMAKE_CURRENT_SOURCE_DIR}/include
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rknn
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/librga 
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/uAPI2
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/common
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/xcore
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/algos
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/iq_parser_v2
                            ${CMAKE_CURRENT_SOURCE_DIR}/include/rkaiq/smartIr
                          )

message(STATUS "model dir PATH: ${EXAMPLE_DIR}/model")

set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install/${LIBC_TYPE}/${PROJECT_NAME}_demo")
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX})
SET(MODEL_DIR "${EXAMPLE_DIR}/model")
file(GLOB MODEL_FILES "${MODEL_DIR}/*")  
install(FILES ${MODEL_FILES} DESTINATION ${CMAKE_INSTALL_PREFIX}/model)