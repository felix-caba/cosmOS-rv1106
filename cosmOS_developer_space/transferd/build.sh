#!/bin/bash

# Build script for transferd using CMake
# Author: felix@cosmOS

set -e

BUILD_TYPE=${1:-Release}  # Changed to Release by default
BUILD_DIR="build"
INSTALL_DIR="$(pwd)/install"

echo "Building transferd with CMake..."
echo "Build type: $BUILD_TYPE"
echo "Install dir: $INSTALL_DIR"

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

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake with proper install prefix and size optimization
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
      -DCMAKE_C_FLAGS_RELEASE="-Os -DNDEBUG" \
      -DCMAKE_CXX_FLAGS_RELEASE="-Os -DNDEBUG" \
      ..

# Build
make -j$(nproc)

# Install
make install

echo "transferd built successfully!"
echo "Executable location: $INSTALL_DIR/bin/transferd"

if [ -f "$INSTALL_DIR/bin/transferd" ]; then
    echo ""
    echo "Binary information:"
    ls -la "$INSTALL_DIR/bin/transferd"
    file "$INSTALL_DIR/bin/transferd"
    echo ""
    echo "Binary size comparison:"
    echo "Stripped size: $(stat -c%s "$INSTALL_DIR/bin/transferd") bytes"
else
    echo "ERROR: Executable not found at expected location!"
    echo "Checking build directory for executable..."
    find . -name "transferd" -type f -executable
fi