#!/bin/bash

# Build and Deploy Script for transferd and YOLO
# Author: felix@cosmOS
# Description: Compiles both transferd and YOLO, then deploys to target device

set -e  # Exit on any error

# Configuration
TARGET_IP="192.168.1.161"
TARGET_USER="root"
TARGET_BIN_DIR="/usr/bin"
WORKSPACE_DIR="/home/felix/docker-shared/cosmOS-rv1106/cosmOS_developer_space"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_target_connectivity() {
    print_status "Checking connectivity to target device $TARGET_USER@$TARGET_IP..."
    if ping -c 1 -W 3 $TARGET_IP > /dev/null 2>&1; then
        print_success "Target device is reachable"
    else
        print_error "Cannot reach target device $TARGET_IP"
        exit 1
    fi
}

build_transferd() {
    print_status "Building transferd..."
    cd "$WORKSPACE_DIR/transferd"
    
    if [ -f "Makefile" ]; then
        make clean
        print_status "Cleaned previous transferd build"
    else
        print_error "Makefile not found in transferd directory"
        exit 1
    fi
    
    # Set TOOLCHAIN_BASE_NAME for uclibc build
    export TOOLCHAIN_BASE_NAME="arm-rockchip830-linux-uclibcgnueabihf"

    make all
    
    if [ -f "transferd" ]; then
        print_success "transferd compiled successfully"
        ls -la transferd
    else
        print_error "transferd compilation failed"
        exit 1
    fi
}


build_yolo() {
    print_status "Building YOLO..."
    cd "$WORKSPACE_DIR/yolo"
    
    # Clean previous build
    if [ -d "build" ]; then
        rm -rf build
        print_status "Cleaned previous YOLO build"
    fi
    
    if [ -d "install" ]; then
        rm -rf install
        print_status "Cleaned previous YOLO install"
    fi
    
    # Build YOLO using build.sh
    if [ -f "build.sh" ]; then
        chmod +x build.sh
        ./build.sh
        
        # Check if YOLO executable was created
        YOLO_EXECUTABLE="$WORKSPACE_DIR/yolo/install/uclibc/luckfox_pico_rtsp_yolov5_demo/luckfox_pico_rtsp_yolov5"
        if [ -f "$YOLO_EXECUTABLE" ]; then
            print_success "YOLO compiled successfully"
            ls -la "$YOLO_EXECUTABLE"
        else
            print_error "YOLO compilation failed - executable not found at $YOLO_EXECUTABLE"
            print_status "Checking install directory contents:"
            find "$WORKSPACE_DIR/yolo/install" -name "*yolo*" -o -name "*luckfox*" 2>/dev/null || true
            exit 1
        fi
    else
        print_error "build.sh not found in YOLO directory"
        exit 1
    fi
}


deploy_files() {
    print_status "Deploying files to target device..."
    
    TRANSFERD_BINARY="$WORKSPACE_DIR/transferd/transferd"
    YOLO_BINARY="$WORKSPACE_DIR/yolo/install/uclibc/luckfox_pico_rtsp_yolov5_demo/luckfox_pico_rtsp_yolov5"
    
    # Check if binaries exist
    if [ ! -f "$TRANSFERD_BINARY" ]; then
        print_error "transferd binary not found at $TRANSFERD_BINARY"
        exit 1
    fi
    
    if [ ! -f "$YOLO_BINARY" ]; then
        print_error "YOLO binary not found at $YOLO_BINARY"
        exit 1
    fi
    
    print_status "Stopping transferd daemon on target (if running)..."
    ssh -i ~/.ssh/id_ed25519 $TARGET_USER@$TARGET_IP "if [ -f /usr/bin/transferd ]; then /usr/bin/transferd stop 2>/dev/null || true; fi" || true
    
    print_status "Deploying transferd to $TARGET_USER@$TARGET_IP:$TARGET_BIN_DIR/transferd"
    scp -i ~/.ssh/id_ed25519 "$TRANSFERD_BINARY" "$TARGET_USER@$TARGET_IP:$TARGET_BIN_DIR/transferd"
    ssh -i ~/.ssh/id_ed25519 $TARGET_USER@$TARGET_IP "chmod +x $TARGET_BIN_DIR/transferd"
    print_success "transferd deployed successfully"

    print_status "Deploying YOLO to $TARGET_USER@$TARGET_IP:$TARGET_BIN_DIR/YOLO"
    scp -i ~/.ssh/id_ed25519 "$YOLO_BINARY" "$TARGET_USER@$TARGET_IP:$TARGET_BIN_DIR/YOLO"
    ssh -i ~/.ssh/id_ed25519 $TARGET_USER@$TARGET_IP "chmod +x $TARGET_BIN_DIR/YOLO"
    print_success "YOLO deployed successfully"
   
    YOLO_MODEL_DIR="$WORKSPACE_DIR/yolo/example/luckfox_pico_rtsp_yolov5/model"
    if [ -d "$YOLO_MODEL_DIR" ]; then
        print_status "Deploying YOLO model files..."
        ssh -i ~/.ssh/id_ed25519 $TARGET_USER@$TARGET_IP "mkdir -p /usr/share/yolo/model"
        scp -i ~/.ssh/id_ed25519 -r "$YOLO_MODEL_DIR"/* "$TARGET_USER@$TARGET_IP:/usr/share/yolo/model/"
        print_success "YOLO model files deployed"
    fi
    
    print_status "Setting up transferd configuration..."
    ssh -i ~/.ssh/id_ed25519 $TARGET_USER@$TARGET_IP "
        if [ ! -f /etc/transferd.conf ]; then
            echo '# Transferd Configuration File' > /etc/transferd.conf
            echo 'source_type=YOLO' >> /etc/transferd.conf
            echo 'Configuration file created at /etc/transferd.conf'
        else
            echo 'Configuration file already exists at /etc/transferd.conf'
        fi
    "
    
    # Show deployed files info
    print_status "Verifying deployed files on target:"
    ssh -i ~/.ssh/id_ed25519 $TARGET_USER@$TARGET_IP "
        echo 'Deployed binaries:'
        ls -la $TARGET_BIN_DIR/transferd $TARGET_BIN_DIR/YOLO 2>/dev/null || echo 'Some binaries not found'
        echo ''
        echo 'Available disk space:'
        df -h $TARGET_BIN_DIR
    "
}

show_usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -t, --target IP     Set target IP address (default: $TARGET_IP)"
    echo "  -u, --user USER     Set target username (default: $TARGET_USER)"
    echo "  --transferd-only    Build and deploy only transferd"
    echo "  --yolo-only         Build and deploy only YOLO"
    echo "  --no-deploy         Build only, skip deployment"
    echo "  --deploy-only       Deploy only, skip building"
}

main() {
    local build_transferd=true
    local build_yolo=true
    local deploy=true
    local build=true
    
 
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -t|--target)
                TARGET_IP="$2"
                shift 2
                ;;
            -u|--user)
                TARGET_USER="$2"
                shift 2
                ;;
            --transferd-only)
                build_yolo=false
                shift
                ;;
            --yolo-only)
                build_transferd=false
                shift
                ;;
            --no-deploy)
                deploy=false
                shift
                ;;
            --deploy-only)
                build=false
                shift
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    print_status "Starting build and deployment process..."
    print_status "Target: $TARGET_USER@$TARGET_IP"
    print_status "Workspace: $WORKSPACE_DIR"
    echo ""
    
    # Check workspace directory
    if [ ! -d "$WORKSPACE_DIR" ]; then
        print_error "Workspace directory not found: $WORKSPACE_DIR"
        exit 1
    fi
    
    # Build phase
    if [ "$build" = true ]; then
        if [ "$build_transferd" = true ]; then
            build_transferd
            echo ""
        fi
        
        if [ "$build_yolo" = true ]; then
            build_yolo
            echo ""
        fi
    fi
    
    # Deployment phase
    if [ "$deploy" = true ]; then
        check_target_connectivity
        deploy_files
    fi
    
    print_success "Build and deployment completed successfully!"
    
    if [ "$deploy" = true ]; then
        echo ""
        print_status "You can now start the transferd daemon on the target with:"
        echo "  ssh $TARGET_USER@$TARGET_IP"
        echo "  /usr/bin/transferd start"
        echo ""
        print_status "Monitor logs with:"
        echo "  /usr/bin/transferd -d"
    fi
}

# Check if script is being sourced or executed
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
