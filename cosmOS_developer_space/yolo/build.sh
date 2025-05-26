#!/bin/bash

ROOT_PWD=$(cd "$(dirname $0)" && cd -P "$(dirname "$SOURCE")" && pwd)

if [ "$1" = "clean" ]; then
	if [ -d "${ROOT_PWD}/build" ]; then
		rm -rf "${ROOT_PWD}/build"
		echo " ${ROOT_PWD}/build has been deleted!"
	fi

	if [ -d "${ROOT_PWD}/install" ]; then
		rm -rf "${ROOT_PWD}/install"
		echo " ${ROOT_PWD}/install has been deleted!"
	fi

	exit
fi

src_dir="example/luckfox_pico_rtsp_yolov5"
	if [[ -d "$src_dir" ]]; then
		if [ -d ${ROOT_PWD}/build ]; then
				rm -rf ${ROOT_PWD}/build
		fi
		mkdir ${ROOT_PWD}/build
		cd ${ROOT_PWD}/build
		cmake .. -DEXAMPLE_DIR="$src_dir" -DEXAMPLE_NAME="luckfox_pico_rtsp_yolov5" -DLIBC_TYPE="uclibc"
		make install
		else
			echo "Error: Directory $src_dir does not exist!"
		fi
		
