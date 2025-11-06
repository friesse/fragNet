#!/bin/bash
# Install dependencies for Game Coordinator Server on Debian 12/13
# Run with: sudo ./install-deps-debian.sh

set -e

echo "========================================"
echo " Game Coordinator - Debian Dependencies"
echo "========================================"
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "ERROR: Please run as root (use sudo)"
    exit 1
fi

echo "[1/6] Updating package lists..."
apt-get update

echo ""
echo "[2/6] Installing build tools..."
apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config

echo ""
echo "[3/6] Installing MariaDB client library..."
apt-get install -y \
    libmariadb-dev \
    libmariadb-dev-compat

echo ""
echo "[4/6] Installing Crypto++ library..."
apt-get install -y libcrypto++-dev

echo ""
echo "[5/6] Installing Protobuf..."
apt-get install -y \
    libprotobuf-dev \
    protobuf-compiler

echo ""
echo "[6/6] Installing additional libraries..."
apt-get install -y \
    libssl-dev \
    zlib1g-dev

echo ""
echo "========================================"
echo " Dependencies installed successfully!"
echo "========================================"
echo ""
echo "Installed packages:"
echo "  - build-essential (GCC, G++, make)"
echo "  - CMake (build system)"
echo "  - MariaDB client library"
echo "  - Crypto++ (cryptopp)"
echo "  - Protobuf"
echo "  - OpenSSL development files"
echo "  - zlib compression library"
echo ""
echo "MANUAL STEP REQUIRED:"
echo "  Download Steam SDK from: https://partner.steamgames.com/"
echo "  Extract to: $(dirname "$0")/steamworks/sdk/"
echo ""
echo "To build, run: ./build-linux.sh"
echo ""
