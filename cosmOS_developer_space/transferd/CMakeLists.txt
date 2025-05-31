cmake_minimum_required(VERSION 3.10)

set(LIBC_TYPE "uclibc")
set(LUCKFOX_SDK_PATH "/home/felix/docker-shared/cosmOS-rv1106/")
set(SDK_PATH "${LUCKFOX_SDK_PATH}")
set(GLIBC_COMPILER $ENV{GLIBC_COMPILER})

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
        
        # Set the correct sysroot
        set(CMAKE_SYSROOT "${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}/sysroot")
        set(CMAKE_FIND_ROOT_PATH "${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}/sysroot")
        
        # Configure find paths
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
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

project(transferd)

add_compile_options(-Wall -g)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

include_directories(SYSTEM 
        "${CMAKE_SYSROOT}/usr/include"
        "${TOOLCHAIN_PATH}/${TOOLCHAIN_PREFIX}/include"
    )

# Find required packages
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Source files
set(SRCS 
    src/screen.c
    src/config.c
    src/daemon.c
    src/log.c
    src/transferd.c
    src/api.c
)

add_executable(${PROJECT_NAME} ${SRCS})

target_link_libraries(${PROJECT_NAME} 
    Threads::Threads
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Stripping ${PROJECT_NAME}")
endif()

# Install target
install(TARGETS ${PROJECT_NAME} DESTINATION bin)