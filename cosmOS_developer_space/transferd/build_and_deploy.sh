#!/bin/bash

# Build and Deploy script for cosmOS applications
# Author: felix@cosmOS

set -e

REMOTE_HOST="root@192.168.1.164"
REMOTE_PATH="/usr/bin"
BASE_DIR="/home/felix/docker-shared/cosmOS-rv1106/cosmOS_developer_space"

echo "=== cosmOS Build and Deploy Script ==="
echo "Target: $REMOTE_HOST:$REMOTE_PATH"
echo ""

check_ssh() {
    echo "Checking SSH connectivity to $REMOTE_HOST..."
    if ssh -o ConnectTimeout=5 -o BatchMode=yes $REMOTE_HOST "echo 'SSH connection successful'" 2>/dev/null; then
        echo "✓ SSH connection OK"
        return 0
    else
        echo "✗ SSH connection failed"
        return 1
    fi
}

# Function to build transferd
build_transferd() {
    echo ""
    echo "=== Building transferd ==="
    cd "$BASE_DIR/transferd"
    
    if [ ! -f "build.sh" ]; then
        echo "✗ transferd build.sh not found"
        return 1
    fi
    
    ./build.sh Release
    
    if [ -f "install/bin/transferd" ]; then
        echo "✓ transferd built successfully"
        return 0
    else
        echo "✗ transferd build failed"
        return 1
    fi
}

# Function to build YOLO
build_yolo() {
    echo ""
    echo "=== Building YOLO ==="
    cd "$BASE_DIR/yolo"
    
    if [ ! -f "build.sh" ]; then
        echo "✗ YOLO build.sh not found"
        return 1
    fi
    
    ./build.sh Release
    
    if [ -f "install/uclibc/luckfox_pico_rtsp_yolov5_demo/luckfox_pico_rtsp_yolov5" ]; then
        echo "✓ YOLO built successfully"
        return 0
    else
        echo "✗ YOLO build failed"
        return 1
    fi
}

# Function to deploy files
deploy_files() {
    echo ""
    echo "=== Deploying files ==="
    
    # Deploy transferd
    echo "Deploying transferd..."
    if scp "$BASE_DIR/transferd/install/bin/transferd" "$REMOTE_HOST:$REMOTE_PATH/" 2>/dev/null; then
        echo "✓ transferd deployed"
    else
        echo "✗ transferd deployment failed"
        return 1
    fi
    
    # Deploy YOLO
    echo "Deploying luckfox_pico_rtsp_yolov5..."
    if scp "$BASE_DIR/yolo/install/uclibc/luckfox_pico_rtsp_yolov5_demo/luckfox_pico_rtsp_yolov5" "$REMOTE_HOST:$REMOTE_PATH/" 2>/dev/null; then
        echo "✓ luckfox_pico_rtsp_yolov5 deployed"
    else
        echo "✗ luckfox_pico_rtsp_yolov5 deployment failed"
        return 1
    fi
    
    return 0
}

# Function to verify deployment
verify_deployment() {
    echo ""
    echo "=== Verifying deployment ==="
    
    # Check transferd
    if ssh $REMOTE_HOST "test -f $REMOTE_PATH/transferd && echo 'transferd found'" 2>/dev/null | grep -q "transferd found"; then
        echo "✓ transferd verified on target"
        ssh $REMOTE_HOST "ls -la $REMOTE_PATH/transferd" 2>/dev/null
    else
        echo "✗ transferd not found on target"
        return 1
    fi
    
    # Check YOLO
    if ssh $REMOTE_HOST "test -f $REMOTE_PATH/luckfox_pico_rtsp_yolov5 && echo 'yolo found'" 2>/dev/null | grep -q "yolo found"; then
        echo "✓ luckfox_pico_rtsp_yolov5 verified on target"
        ssh $REMOTE_HOST "ls -la $REMOTE_PATH/luckfox_pico_rtsp_yolov5" 2>/dev/null
    else
        echo "✗ luckfox_pico_rtsp_yolov5 not found on target"
        return 1
    fi
    
    return 0
}

# Main execution
main() {
    # Check SSH first
    if ! check_ssh; then
        echo "Aborting: Cannot connect to target device"
        exit 1
    fi
    
    # Build applications
    if ! build_transferd; then
        echo "Aborting: transferd build failed"
        exit 1
    fi
    
    if ! build_yolo; then
        echo "Aborting: YOLO build failed"
        exit 1
    fi
    
    # Deploy files
    if ! deploy_files; then
        echo "Aborting: Deployment failed"
        exit 1
    fi
    
    # Verify deployment
    if ! verify_deployment; then
        echo "Warning: Deployment verification failed"
        exit 1
    fi
    
    echo ""
    echo "=== Deployment Complete ==="
    echo "✓ All applications built and deployed successfully!"
    echo "✓ Files are available at $REMOTE_HOST:$REMOTE_PATH/"
}

# Run main function
main "$@"