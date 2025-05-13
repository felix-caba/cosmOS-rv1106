#!/bin/bash

# Colores
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

GLOBAL_OEM_NAME=oem
GLOBAL_FS_TYPE_SUFFIX=_fs_type
GLOBAL_INITRAMFS_BOOT_NAME=""
GLOBAL_PARTITIONS=""
GLOBAL_SDK_VERSION=""
GLOBAL_ROOT_FILESYSTEM_NAME=rootfs



export RK_POST_OVERLAY="overlay-luckfox-config overlay-luckfox-buildroot-init overlay-luckfox-buildroot-shadow"

export RK_CAMERA_SENSOR_IQFILES="sc4336_OT01_40IRC_F16.json sc3336_CMK-OT2119-PC1_30IRC-F16.json mis5001_CMK-OT2115-PC1_30IRC-F16.json"
export RK_CAMERA_SENSOR_CAC_BIN="CAC_sc4336_OT01_40IRC_F16"
export RK_BUILD_APP_TO_OEM_PARTITION=y

export RK_BUILD_VERSION_TYPE=PRODUCTION
export RK_APP_TYPE=RKIPC_RV1106
export RK_APP_CROSS="arm-rockchip830-linux-uclibcgnueabihf"

export RK_BOOT_MEDIUM=sd_card

PROJECT_TOP_DIR=$(pwd)
ROOT=${PROJECT_TOP_DIR}

export SDK_APP_DIR="${ROOT}/project/app"
export SDK_MEDIA_DIR="${ROOT}/media"
export SDK_SYSDRV_DIR="${ROOT}/sysdrv"

export RK_TOOLCHAIN_CROSS=arm-rockchip830-linux-uclibcgnueabihf
export RK_PROJECT_TOOLCHAIN_CROSS=$RK_TOOLCHAIN_CROSS
export RK_PROJECT_PATH_PC_TOOLS=$ROOT/output/out/sysdrv_out/pc

##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS
export RK_APP_OUTPUT="${SDK_APP_DIR}/output"

export RK_PROJECT_OUTPUT="${ROOT}/output/out"
export RK_PROJECT_PATH_APP="${RK_PROJECT_OUTPUT}/app_out"
export RK_PROJECT_PATH_MEDIA="${RK_PROJECT_OUTPUT}/media_out"
export RK_PROJECT_PATH_SYSDRV="${RK_PROJECT_OUTPUT}/sysdrv_out"

export RK_PROJECT_PACKAGE_USERDATA_DIR=$RK_PROJECT_OUTPUT/userdata

export RK_PROJECT_PACKAGE_OEM_DIR=$RK_PROJECT_OUTPUT/oem
##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS ##### OUTPUTS

export RK_PROJECT_PACKAGE_ROOTFS_DIR=${RK_PROJECT_OUTPUT}/rootfs_uclibc_rv1106

export RK_PROJECT_OUTPUT_IMAGE="${ROOT}/output/image"
export RK_PARTITION_FS_TYPE_CFG="rootfs@IGNORE@ext4,userdata@/userdata@ext4,oem@/oem@ext4"
export RK_PARTITION_CMD_IN_ENV="32K(env),512K@32K(idblock),256K(uboot),32M(boot),512M(oem),256M(userdata),6G(rootfs)"

export RK_PRE_BUILD_OEM_SCRIPT=luckfox-buildroot-oem-pre.sh
export RK_PRE_BUILD_USERDATA_SCRIPT=luckfox-userdata-pre.sh

export RK_PROJECT_FILE_OEM_SCRIPT=$RK_PROJECT_OUTPUT/S21appinit
export RK_PROJECT_FILE_ROOTFS_SCRIPT="${RK_PROJECT_OUTPUT}/out/S20linkmount"


// chmod +x "${RK_PROJECT_PATH_PC_TOOLS}/mkfs_ext4.sh" #
export RK_PROJECT_TOOLS_MKFS_EXT4="${RK_PROJECT_PATH_PC_TOOLS}/mkfs_ext4.sh" # Corrected: Include full path
export RK_JOBS=3

function msg_info() {
    echo -e "${GREEN}Info: $1${NC}"
}

function msg_error() {
    echo -e "${RED}Error: $1${NC}"
}

function msg_warn() {
	echo -e "${C_YELLOW}[$(basename $0):warn] $1${C_NORMAL}"
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

###################################################################################################################
###################################################################################################################

function parse_partition_env() {
	local env_all_flag env_final env_final_offset
	local part_size part_offset part_name part_final partitions tmp_part_offset tmp_part_offset_b16
	local part_size_bytes part_offset_bytes size_final_char offset_final_char part_size_bytes_b16

	if [[ -z $RK_PARTITION_CMD_IN_ENV ]]; then
		msg_error "No found partition, please check RK_PARTITION_CMD_IN_ENV in BoardConfig: $BOARD_CONFIG"
		exit 1
	fi

	# format be like: "4M(uboot),32K(env),32M(boot),1G(rootfs),-(userdata)"
	IFS=,
	env_all_flag=0
	tmp_part_offset=0

	read -ra env_arr <<<"$RK_PARTITION_CMD_IN_ENV"
	env_final=${env_arr[-1]}
	if [[ $env_final =~ "@" ]]; then
		env_final_offset=$(echo $env_final | cut -s -d '(' -f1 | cut -s -d '@' -f2)
		if [[ $((env_final_offset)) == 0 ]]; then
			env_all_flag=1
		fi
	fi
	for part in $RK_PARTITION_CMD_IN_ENV; do
		part_size=$(echo $part | cut -s -d '(' -f1 | cut -d '@' -f1)
		part_name=$(echo $part | cut -s -d '(' -f2 | cut -s -d ')' -f1)
		part_final=$(echo $part | cut -s -d '(' -f2 | cut -s -d ')' -f2)

		if [[ -z $part_size || -z $part_name || -n $part_final ]]; then
			msg_error "Parse partition failed, exit !!!"
			msg_error "Please check the partition format: $RK_PARTITION_CMD_IN_ENV"
			exit 1
		fi

		# parse offset
		if [[ $part =~ "@" ]]; then
			part_offset=$(echo $part | cut -s -d '(' -f1 | cut -s -d '@' -f2)
			offset_final_char=${part_offset: -1}
			case $offset_final_char in
			K | k)
				part_offset=$((${part_offset/%${offset_final_char}/}))
				part_offset_bytes=$(($part_offset * 1024))
				;;
			M | m)
				part_offset=$((${part_offset/%${offset_final_char}/}))
				part_offset_bytes=$(($part_offset * 1024 * 1024))
				;;
			G | g)
				part_offset=$((${part_offset/%${offset_final_char}/}))
				part_offset_bytes=$(($part_offset * 1024 * 1024 * 1024))
				;;
			T | t)
				part_offset=$((${part_offset/%${offset_final_char}/}))
				part_offset_bytes=$(($part_offset * 1024 * 1024 * 1024 * 1024))
				;;
			P | p)
				part_offset=$((${part_offset/%${offset_final_char}/}))
				part_offset_bytes=$(($part_offset * 1024 * 1024 * 1024 * 1024 * 1024))
				;;
			E | e)
				part_offset=$((${part_offset/%${offset_final_char}/}))
				part_offset_bytes=$(($part_offset * 1024 * 1024 * 1024 * 1024 * 1024 * 1024))
				;;
			-)
				if [[ ${#part_offset} != 1 ]]; then
					msg_error "Partition($part_name) offset($part_offset) error, exit !!!"
					exit 1
				fi
				part_offset_bytes=$part_offset
				;;
			*)
				part_offset_bytes=$(($part_offset))
				if [[ $part_offset_bytes == 0 && "$part_offset" != "0" && "$part_offset" != "0x0" ]]; then
					msg_error "Partition '$part_name' offset '$part_offset' error, exit !!!"
					exit 1
				fi
				;;
			esac
		else
			part_offset_bytes=
		fi

		# parse partition size
		size_final_char=${part_size: -1}
		case $size_final_char in
		K | k)
			part_size=$((${part_size/%${size_final_char}/}))
			part_size_bytes=$(($part_size * 1024))
			;;
		M | m)
			part_size=$((${part_size/%${size_final_char}/}))
			part_size_bytes=$(($part_size * 1024 * 1024))
			;;
		G | g)
			part_size=$((${part_size/%${size_final_char}/}))
			part_size_bytes=$(($part_size * 1024 * 1024 * 1024))
			;;
		T | t)
			part_size=$((${part_size/%${size_final_char}/}))
			part_size_bytes=$(($part_size * 1024 * 1024 * 1024 * 1024))
			;;
		P | p)
			part_size=$((${part_size/%${size_final_char}/}))
			part_size_bytes=$(($part_size * 1024 * 1024 * 1024 * 1024 * 1024))
			;;
		E | e)
			part_size=$((${part_size/%${size_final_char}/}))
			part_size_bytes=$(($part_size * 1024 * 1024 * 1024 * 1024 * 1024 * 1024))
			;;
		-)
			if [[ ${#part_size} != 1 ]]; then
				msg_error "Partition($part_name) size($part_size) error, exit !!!"
				exit 1
			fi
			part_size_bytes=$part_size
			;;
		*)
			part_size_bytes=$(($part_size))
			;;
		esac

		# Judge the validity of parameters
		if [[ $part_size_bytes == 0 ]]; then
			msg_error "Error: partition($part_name) size equal to 0, exit !!!"
			exit 1
		fi
		if [[ -n "${part_offset_bytes}" ]]; then
			if [[ "$part" == "$env_final" && "$env_all_flag" == 1 ]]; then
				tmp_part_offset=$part_offset_bytes
			else
				if [[ $((part_offset_bytes)) -ge $((tmp_part_offset)) ]]; then
					tmp_part_offset=$part_offset_bytes
				else
					msg_error "Partition($part_name) offset set too small, exit !!!"
					exit 1
				fi
			fi
		fi

		# Convert base 10 to base 16
		if [[ $part_size_bytes =~ "-" ]]; then
			part_size_bytes_b16="-"
		else
			part_size_bytes_b16="0x$(echo "obase=16;$part_size_bytes" | bc)"
		fi
		if [[ $tmp_part_offset =~ "-" ]]; then
			tmp_part_offset_b16="-"
		else
			tmp_part_offset_b16="0x$(echo "obase=16;$tmp_part_offset" | bc)"
		fi
		if [ "$tmp_part_offset_b16" = "0x" ]; then
			tmp_part_offset_b16="0"
		fi

		partitions="$partitions,$part_size_bytes_b16@$tmp_part_offset_b16($part_name)"
		[[ $part_size_bytes =~ "-" || $tmp_part_offset =~ "-" ]] || tmp_part_offset=$((tmp_part_offset + part_size_bytes))
	done
	IFS=
	GLOBAL_PARTITIONS="${partitions/,/}"
	echo "GLOBAL_PARTITIONS: $GLOBAL_PARTITIONS"
}

function build_tool() {
	test -d ${SDK_SYSDRV_DIR} && make pctools -C ${SDK_SYSDRV_DIR}
	cp -fa $PROJECT_TOP_DIR/project/scripts/mk-fitimage.sh $RK_PROJECT_PATH_PC_TOOLS
	cp -fa $PROJECT_TOP_DIR/project/scripts/compress_tool $RK_PROJECT_PATH_PC_TOOLS
	cp -fa $PROJECT_TOP_DIR/project/scripts/mk-tftp_sd_update.sh $RK_PROJECT_PATH_PC_TOOLS

	finish_build
}

###################################################################################################################
###################################################################################################################
function __COPY_FILES() {
	mkdir -p "$2"
	if [ -d "$1" ]; then
		cp -rfa $1/* $2
	else
		msg_warn "Please check path [$1] [$2] again"
	fi
}

function get_partition_size() {
	local target_part_name partitions
	target_part_name=$1
	partitions=${GLOBAL_PARTITIONS}
	if [ -z "$target_part_name" -o -z "$partitions" ]; then
		msg_error "Invalid paramter, exit !!!"
		exit 1
	fi

	IFS=,
	local part_size part_name part_size_bytes
	for part in $partitions; do
		part_size=$(echo $part | cut -d '@' -f1)
		part_name=$(echo $part | cut -d '(' -f2 | cut -d ')' -f1)

		[[ $part_size =~ "-" ]] && continue

		part_size_bytes=$(($part_size))

		if [ "${part_name%_[ab]}" == "${target_part_name%_[ab]}" ]; then
			echo "$part_size_bytes"
			IFS=
			return
		fi
	done
	IFS=

	echo "0"
	return
}

function __PACKAGE_USERDATA() {
	local media_userdata app_userdata
	userdata_media="$RK_PROJECT_PATH_MEDIA/install_to_userdata"
	userdata_app="$RK_PROJECT_PATH_APP/install_to_userdata"

	if [ -d "$userdata_media" ]; then
		if [ "$(ls -A $userdata_media)" ]; then
			cp -rfa $RK_PROJECT_PATH_MEDIA/install_to_userdata/* $RK_PROJECT_PACKAGE_USERDATA_DIR
		fi
	fi
	if [ -d "$userdata_app" ]; then
		if [ "$(ls -A $userdata_app)" ]; then
			cp -rfa $RK_PROJECT_PATH_MEDIA/install_to_userdata/* $RK_PROJECT_PACKAGE_USERDATA_DIR
		fi
	fi
}

function __PACKAGE_ROOTFS() {
    echo "============Start packaging rootfs============"
    echo "=====Will fail if rootf hasnt been built======"
    echo "=============================================="

    local rootfs_tarball rootfs_out_dir
	rootfs_tarball="$RK_PROJECT_PATH_SYSDRV/rootfs_uclibc_rv1106.tar"

    if [ ! -f $rootfs_tarball ]; then
		msg_error "Build rootfs is not yet complete, packaging cannot proceed!"
		exit 0
	fi

	__COPY_FILES $RK_PROJECT_PATH_APP/root $RK_PROJECT_PACKAGE_ROOTFS_DIR

	__COPY_FILES $RK_PROJECT_PATH_MEDIA/root $RK_PROJECT_PACKAGE_ROOTFS_DIR
	__COPY_FILES $ROOT/external $RK_PROJECT_PACKAGE_ROOTFS_DIR

	#Extract rootfs tarball to out
	rootfs_out_dir=$RK_PROJECT_OUTPUT/
	mkdir -p $rootfs_out_dir
	tar -xf $rootfs_tarball -C $rootfs_out_dir

	if [ -d "$RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/share/iqfiles" ]; then
		(
			cd $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc
			ln -sf ../usr/share/iqfiles ./
		)
	fi


    if [ -f $RK_PROJECT_FILE_ROOTFS_SCRIPT ]; then
		chmod a+x $RK_PROJECT_FILE_ROOTFS_SCRIPT
		cp -f $RK_PROJECT_FILE_ROOTFS_SCRIPT $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d
	fi
}

function __PACKAGE_RESOURCES() {
	local _iqfiles_dir _install_dir _target_dir _avs_calib_install_dir _avs_calib_src
	_target_dir="$1"
	if [ ! -d $_target_dir ]; then
		msg_error "Not found target dir: $_target_dir"
		exit 1
	fi

	_install_dir=$_target_dir/usr
	_iqfiles_dir=$_install_dir/share/iqfiles

	__COPY_FILES $RK_PROJECT_PATH_SYSDRV/kernel_drv_ko/ $_install_dir/ko

	__COPY_FILES $RK_PROJECT_PATH_APP/bin $_install_dir/bin/
	__COPY_FILES $RK_PROJECT_PATH_APP/lib $_install_dir/lib/
	__COPY_FILES $RK_PROJECT_PATH_APP/share $_install_dir/share/
	__COPY_FILES $RK_PROJECT_PATH_APP/usr $_install_dir/
	__COPY_FILES $RK_PROJECT_PATH_APP/etc $_install_dir/etc/

	__COPY_FILES $RK_PROJECT_PATH_MEDIA/bin $_install_dir/bin/
	__COPY_FILES $RK_PROJECT_PATH_MEDIA/lib $_install_dir/lib/
	__COPY_FILES $RK_PROJECT_PATH_MEDIA/share $_install_dir/share/
	__COPY_FILES $RK_PROJECT_PATH_MEDIA/usr $_install_dir/

	_avs_calib_install_dir=$_install_dir/share/avs_calib
	mkdir -p $_avs_calib_install_dir
	if [ -n "$RK_AVS_CALIB" ]; then
		IFS=" "
		for item in $(echo $RK_AVS_CALIB); do
			_avs_calib_src=$RK_PROJECT_PATH_MEDIA/avs_calib/$item
			if [ -f "$_avs_calib_src" ]; then
				if [[ $_avs_calib_src =~ .*pto$ ]]; then
					cp -rfa $_avs_calib_src $_avs_calib_install_dir/calib_file.pto
				else
					cp -rfa $_avs_calib_src $_avs_calib_install_dir/calib_file.xml
				fi
			fi
		done
		IFS=
	fi

	if [ -n "$RK_AVS_LUT" ]; then
		_avs_lut_src=$(find $RK_PROJECT_PATH_MEDIA -name $RK_AVS_LUT)
		if [ -n "$_avs_lut_src" ]; then
			_avs_lut_install_dir=$_install_dir/share/middle_lut
			mkdir -p $_avs_lut_install_dir
			cp -rfa $_avs_lut_src $_avs_lut_install_dir/
		fi
	fi

	mkdir -p $_iqfiles_dir
	if [ -n "$RK_CAMERA_SENSOR_IQFILES" ]; then
		IFS=" "
		for item in $(echo $RK_CAMERA_SENSOR_IQFILES); do
			if [ -f "$RK_PROJECT_PATH_MEDIA/isp_iqfiles/$item" ]; then
				cp -rfa $RK_PROJECT_PATH_MEDIA/isp_iqfiles/$item $_iqfiles_dir
			fi
		done
		IFS=
	else
		msg_warn "Not found RK_CAMERA_SENSOR_IQFILES on the $(realpath $BOARD_CONFIG), copy all default for emmc, sd-card or nand"
		if [ "$RK_BOOT_MEDIUM" != "spi_nor" ]; then
			cp -rfa $RK_PROJECT_PATH_MEDIA/isp_iqfiles/* $_iqfiles_dir
		fi
	fi

	if [ -n "$RK_CAMERA_SENSOR_CAC_BIN" ]; then
		IFS=" "
		for item in $(echo $RK_CAMERA_SENSOR_CAC_BIN); do
			if [ -d "$RK_PROJECT_PATH_MEDIA/isp_iqfiles/$item" ]; then
				cp -rfa $RK_PROJECT_PATH_MEDIA/isp_iqfiles/$item $_iqfiles_dir
			fi
		done
		IFS=
	fi
}

function __PACKAGE_OEM() {
	mkdir -p $RK_PROJECT_PACKAGE_OEM_DIR
	__PACKAGE_RESOURCES $RK_PROJECT_PACKAGE_OEM_DIR
	if [ -d "$RK_PROJECT_PACKAGE_OEM_DIR/usr/share/iqfiles" ]; then
		(
			cd $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc
			ln -sf ../oem/usr/share/iqfiles ./
		)
	fi

	mkdir -p $(dirname $RK_PROJECT_FILE_OEM_SCRIPT)
	cat >$RK_PROJECT_FILE_OEM_SCRIPT <<EOF
#!/bin/sh
[ -f /etc/profile.d/RkEnv.sh ] && source /etc/profile.d/RkEnv.sh
case \$1 in
	start)
		sh /oem/usr/bin/RkLunch.sh
		;;
	stop)
		sh /oem/usr/bin/RkLunch-stop.sh
		;;
	*)
		exit 1
		;;
esac
EOF
	chmod a+x $RK_PROJECT_FILE_OEM_SCRIPT
	cp -f $RK_PROJECT_FILE_OEM_SCRIPT $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/init.d
}

function __BUILD_ENABLE_COREDUMP_SCRIPT() {
	local tmp_path coredump2sdcard
	rm -f $RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/profile.d/enable_coredump.sh

	tmp_path=$RK_PROJECT_PACKAGE_ROOTFS_DIR/etc/profile.d/enable_coredump.sh
	coredump2sdcard="$RK_PROJECT_PACKAGE_ROOTFS_DIR/usr/bin/coredump2sdcard.sh"
	cat >$coredump2sdcard <<EOF
#!/bin/sh
exec cat -  > "/mnt/sdcard/core-\$1-\$2"
EOF
	chmod a+x $coredump2sdcard

	cat >$tmp_path <<EOF
#!/bin/sh
if [ "\$(id -u)" = "0" ]; then
ulimit -c unlimited
echo "/data/core-%p-%e" > /proc/sys/kernel/core_pattern
echo "| /usr/bin/coredump2sdcard.sh %p %e" > /proc/sys/kernel/core_pattern
fi
EOF
	chmod u+x $tmp_path
}

function __RUN_POST_CLEAN_FILES() {
	echo "================================================================================"
	IFS=$RECORD_IFS

	local dir_find todo_files
	if [ -d "$RK_PROJECT_PACKAGE_ROOTFS_DIR" ]; then
		dir_find="$RK_PROJECT_PACKAGE_ROOTFS_DIR"
	fi
	if [ -d "$RK_PROJECT_PACKAGE_OEM_DIR" ]; then
		dir_find="$dir_find $RK_PROJECT_PACKAGE_OEM_DIR"
	fi

	if [ -n "$RK_AUDIO_MODEL" ]; then
		if find $dir_find -type f -name "rkaudio*.rknn" | grep -w $RK_AUDIO_MODEL; then
			echo "Found config RK_AUDIO_MODEL: $RK_AUDIO_MODEL"
			todo_files=$(find $dir_find -name "rkaudio*.rknn" | grep -v $RK_AUDIO_MODEL || echo "")
			if [ -n "$todo_files" ]; then
				rm -rfv $todo_files
			fi
		fi
	fi
	echo "================================================================================"

	if [ -n "$RK_AIISP_MODEL" ]; then
		if find $dir_find -type f -name $RK_AIISP_MODEL | grep -w $RK_AIISP_MODEL; then
			echo "Found config RK_AIISP_MODEL: $RK_AIISP_MODEL"
			todo_files=$(find $dir_find -name "*.aiisp" | grep -v $RK_AIISP_MODEL || echo "")
			if [ -n "$todo_files" ]; then
				rm -rfv $todo_files
			fi
		fi
	fi
	echo "================================================================================"

	if [ -n "$RK_NPU_MODEL" ]; then
		if find $dir_find -type f -name $RK_NPU_MODEL | grep -w $RK_NPU_MODEL; then
			todo_files=$(find $dir_find -type f -name "object*.data" | grep -v $RK_NPU_MODEL)
			if [ -n "$todo_files" ]; then
				rm -rfv $todo_files
			fi

			local target_npu_data
			target_npu_data=$(find $dir_find -type f -name $RK_NPU_MODEL)
			if [ ! "${RK_NPU_MODEL}x" = "object_detection_pfp.datax" ]; then
				if [ "$RK_BUILD_APP_TO_OEM_PARTITION" = "y" ]; then
					mkdir -p $RK_PROJECT_PACKAGE_OEM_DIR/usr/lib
					mv $target_npu_data $RK_PROJECT_PACKAGE_OEM_DIR/usr/lib/object_detection_pfp.data
				else
					mkdir -p $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib
					mv $target_npu_data $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/usr/lib/object_detection_pfp.data
				fi
			fi
		fi
	fi
	echo "================================================================================"

}

function __RUN_PRE_BUILD_OEM_SCRIPT() {
	local tmp_path
	tmp_path="boardconfig/"

	__RUN_POST_CLEAN_FILES

	if [ -f "$tmp_path/$RK_PRE_BUILD_OEM_SCRIPT" ]; then
		bash -x $tmp_path/$RK_PRE_BUILD_OEM_SCRIPT
	fi
}

function __RUN_POST_BUILD_SCRIPT() {
	local tmp_path
	tmp_path="boardconfig/"
	__RUN_POST_CLEAN_FILES
}

function __RUN_POST_BUILD_USERDATA_SCRIPT() {
	local tmp_path
	tmp_path="boardconfig/"
	if [ -f "$tmp_path/$RK_PRE_BUILD_USERDATA_SCRIPT" ]; then
		bash -x $tmp_path/$RK_PRE_BUILD_USERDATA_SCRIPT
	fi
}

function __RELEASE_FILESYSTEM_FILES() {

	if [ "$RK_BUILD_VERSION_TYPE" = "DEBUG" ]; then
		msg_info "RK_BUILD_VERSION_TYPE is DEBUG, ignore strip symbols"
		return
	fi

	local _target_dir
	_target_dir=$1
	msg_info "start to strip $_target_dir"
	if [ -d "$_target_dir" -a -n "${RK_PROJECT_TOOLCHAIN_CROSS}" ]; then
		find "$_target_dir" \( -name "lib*.la" -o -name "lib*.a" \) | xargs rm -rf {}
		rm -rf $(find "$_target_dir" -name pkgconfig)
		find "$_target_dir" -type f \( -perm /111 -o -name '*.so*' \) \
			-not \( -name 'libpthread*.so*' -o -name 'ld-*.so*' -o -name '*.ko' \) -print0 |
			xargs -0 ${RK_PROJECT_TOOLCHAIN_CROSS}-strip 2>/dev/null || true
		find "$_target_dir" -type f -name '*.ko' -print0 |
			xargs -0 ${RK_PROJECT_TOOLCHAIN_CROSS}-strip --strip-debug 2>/dev/null || true
	else
		msg_warn "not found target dir: $_target_dir, ignore"
	fi
}

function build_mkimg() {
	local src dst fs_type part_size part_name
	part_name=$1
	part_size=$(get_partition_size $part_name)
	if [ $((part_size)) -lt 1 ]; then
		msg_warn "Not found partition named [$part_name]"
		msg_warn "Please check RK_PARTITION_CMD_IN_ENV in BoardConfig: $BOARD_CONFIG"
		return
	fi

	if [ -z "$2" -o ! -d "$2" ]; then
		msg_error "Not exist source dir: $2"
		exit 1
	fi

	if [ ! -f $RK_PROJECT_PATH_PC_TOOLS/mk-fitimage.sh \
		-o ! -f $RK_PROJECT_PATH_PC_TOOLS/mkimage \
		-o ! -f $RK_PROJECT_PATH_PC_TOOLS/compress_tool ]; then
		msg_error "Not found mk-fitimage.sh or mkimage or compress_tool in $RK_PROJECT_PATH_PC_TOOLS"
	fi

	src=$2
	dst=$RK_PROJECT_OUTPUT_IMAGE/${part_name}.img

	fs_type=ext4

	if [ "$LF_TARGET_ROOTFS" == "buildroot" ] || [ "$LF_TARGT_ROOTFS" == "busybox" ]; then
		__RELEASE_FILESYSTEM_FILES $src
	fi

	msg_info "src=$src"
	msg_info "dst=$dst"
	msg_info "fs_type=$fs_type"
	msg_info "part_name=$part_name"
	msg_info "part_size=$((part_size / 1024 / 1024))MB"

	case $fs_type in
	ext4)
		$RK_PROJECT_TOOLS_MKFS_EXT4 $src $dst $part_size
		;;
	*)
		touch blostexd
		msg_error "Not support fs type: $fs_type"
		;;
	esac

	finish_build
}

function post_overlay() {
	[ -n "$RK_POST_OVERLAY" ] || return 0

	local tmp_path
	tmp_path=$"boardconfig/"

	for overlay_dir in $RK_POST_OVERLAY; do
		if [ -d "$tmp_path/overlay/$overlay_dir" ]; then
			rsync -a --ignore-times --keep-dirlinks --chmod=u=rwX,go=rX --exclude .empty \
				$tmp_path/overlay/$overlay_dir/* $RK_PROJECT_PACKAGE_ROOTFS_DIR/
		fi
	done
}

function build_tftp_sd_update() {
	# copy tools
	mkdir -p $RK_PROJECT_PATH_PC_TOOLS
	if [ ! -f $RK_PROJECT_PATH_PC_TOOLS/mk-tftp_sd_update.sh ]; then
		build_tool
	fi

	if [ "$RK_BOOT_MEDIUM" = "sd_card" ]; then
		$RK_PROJECT_PATH_PC_TOOLS/mk-tftp_sd_update.sh $GLOBAL_PARTITIONS $RK_PROJECT_OUTPUT_IMAGE emmc
	fi

	finish_build
}

###################################################################################################################
###################################################################################################################
###################################################################################################################
###################################################################################################################
###################################################################################################################

function build_firmware() {
	echo "============Start building firmware============"
	echo "=====Will fail if rootf hasnt been built======"
	echo "=============================================="

	mkdir -p ${RK_PROJECT_OUTPUT_IMAGE}

	echo "Copying S20linkmount to ${RK_PROJECT_OUTPUT}/"
	cp -f S20linkmount "${RK_PROJECT_OUTPUT}/"
	echo "Copying project/scripts/compress_tool to ${RK_PROJECT_PATH_PC_TOOLS}/"
	cp -f project/scripts/compress_tool "${RK_PROJECT_PATH_PC_TOOLS}"
	echo "Copying project/scripts/mk-fitimage.sh to ${RK_PROJECT_PATH_PC_TOOLS}/"
	cp -f project/scripts/mk-fitimage.sh "${RK_PROJECT_PATH_PC_TOOLS}"

	# Puts global variables in the env
	parse_partition_env

	__PACKAGE_ROOTFS
	__PACKAGE_OEM

	__BUILD_ENABLE_COREDUMP_SCRIPT

	__RUN_PRE_BUILD_OEM_SCRIPT

	if [ "$RK_BUILD_APP_TO_OEM_PARTITION" = "y" ]; then
		rm -rf $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem/*
		mkdir -p $RK_PROJECT_PACKAGE_ROOTFS_DIR/oem
		build_mkimg $GLOBAL_OEM_NAME $RK_PROJECT_PACKAGE_OEM_DIR
	fi

	__RUN_POST_BUILD_SCRIPT
	post_overlay


	build_mkimg boot $RK_PROJECT_PACKAGE_ROOTFS_DIR

	build_mkimg $GLOBAL_ROOT_FILESYSTEM_NAME $RK_PROJECT_PACKAGE_ROOTFS_DIR

	# package a empty userdata parition image

	mkdir -p $RK_PROJECT_PACKAGE_USERDATA_DIR
	if [ "$RK_ENABLE_FASTBOOT" != "y" ]; then
		__PACKAGE_USERDATA
		__RUN_POST_BUILD_USERDATA_SCRIPT
	fi

	build_mkimg userdata $RK_PROJECT_PACKAGE_USERDATA_DIR
	build_tftp_sd_update


	finish_build
}

function clean_firmware() {
    echo "============Start cleaning firmware============"
    rm -rf ${RK_PROJECT_OUTPUT_IMAGE}
    rm -rf $RK_PROJECT_PACKAGE_ROOTFS_DIR
    rm -rf $RK_PROJECT_PACKAGE_OEM_DIR
    rm -rf $RK_PROJECT_PACKAGE_USERDATA_DIR
    finish_build
}

case "$1" in
    app)
        build_app
        ;;
    media)
        build_media
        ;;
    firmware)
        build_firmware
        ;;
	tool)
		build_tool
		;;
    clean_app)
        clean_app
        ;;
    clean_media)
        clean_media
        ;;
    clean_firmware)
        clean_firmware
        ;;
    *)
        echo "Usage: $0 {app|media|firmware|clean_app|clean_media|clean_firmware}"
        exit 1
        ;;
esac
