#!/bin/bash
# Build script for Game Coordinator Server on Linux

set -e

echo "========================================"
echo " Game Coordinator Server - Linux Build"
echo "========================================"
echo ""

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found!"
    echo "Install with: sudo apt install cmake"
    exit 1
fi

# Check for g++
if ! command -v g++ &> /dev/null; then
    echo "ERROR: G++ compiler not found!"
    echo "Install with: sudo apt install build-essential"
    exit 1
fi

# Get number of CPU cores for parallel build
CORES=$(nproc)
echo "Using $CORES CPU cores for parallel build"
echo ""

# Create build directory
mkdir -p build-linux
cd build-linux

echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: CMake configuration failed!"
    echo ""
    echo "Common issues:"
    echo "  - Missing dependencies: Run ./install-deps-debian.sh"
    echo "  - Missing Steam SDK: Download from partner.steamgames.com"
    echo "  - Missing MariaDB: sudo apt install libmariadb-dev"
    echo ""
    exit 1
fi

echo ""
echo "Building..."
cmake --build . --config Release -j $CORES

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: Build failed!"
    echo "Check the output above for specific errors."
    echo ""
    exit 1
fi

echo ""
echo "========================================"
echo " Build completed successfully!"
echo "========================================"
echo "Binary location: build-linux/gc_server/gc-server_linux64"
echo ""
echo "To run the server:"
echo "  cd build-linux/gc_server"
echo "  export SteamAppId=730"
echo "  ./gc-server_linux64"
echo ""
