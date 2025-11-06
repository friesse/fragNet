#!/bin/bash

###############################################################################
# Game Coordinator Server Startup Script
###############################################################################

# Configuration
export GC_BIND_IP="${GC_BIND_IP:-0.0.0.0}"  # Bind to all interfaces by default
export GC_PORT="${GC_PORT:-21818}"          # Default GC port

# Colored output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=====================================${NC}"
echo -e "${GREEN}Starting Game Coordinator Server${NC}"
echo -e "${GREEN}=====================================${NC}"
echo ""

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Check if build directory exists
if [ ! -d "$SCRIPT_DIR/build" ]; then
    echo -e "${RED}ERROR: Build directory not found!${NC}"
    echo "Please build the project first:"
    echo "  mkdir build && cd build"
    echo "  cmake .."
    echo "  make"
    exit 1
fi

# Find the gc_server executable
GC_SERVER=""
if [ -f "$SCRIPT_DIR/build/gc_server/gc_server" ]; then
    GC_SERVER="$SCRIPT_DIR/build/gc_server/gc_server"
elif [ -f "$SCRIPT_DIR/build/gc_server" ]; then
    GC_SERVER="$SCRIPT_DIR/build/gc_server"
elif [ -f "$SCRIPT_DIR/gc_server" ]; then
    GC_SERVER="$SCRIPT_DIR/gc_server"
else
    echo -e "${RED}ERROR: gc_server executable not found!${NC}"
    echo "Checked locations:"
    echo "  - $SCRIPT_DIR/build/gc_server/gc_server"
    echo "  - $SCRIPT_DIR/build/gc_server"
    echo "  - $SCRIPT_DIR/gc_server"
    exit 1
fi

echo -e "${GREEN}Found executable: ${NC}$GC_SERVER"
echo ""

# Display configuration
echo -e "${YELLOW}Configuration:${NC}"
echo "  Bind IP:   $GC_BIND_IP"
echo "  Port:      $GC_PORT"
echo ""

# Check if port is already in use
if netstat -tuln 2>/dev/null | grep -q ":${GC_PORT} "; then
    echo -e "${YELLOW}WARNING: Port ${GC_PORT} appears to be in use!${NC}"
    echo "Processes using this port:"
    netstat -tulpn 2>/dev/null | grep ":${GC_PORT} " || echo "  (unable to check - may need sudo)"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Check if Steam SDK libraries are present
if [ -d "$SCRIPT_DIR/steamworks/sdk" ]; then
    echo -e "${GREEN}✓ Steam SDK found${NC}"
else
    echo -e "${YELLOW}⚠ Steam SDK not found at expected location${NC}"
fi

# Set Steam App ID
export SteamAppId=730
echo -e "${GREEN}✓ Set SteamAppId=730 (CS:GO)${NC}"
echo ""

# Create logs directory if it doesn't exist
mkdir -p "$SCRIPT_DIR/logs"

# Generate log filename with timestamp
LOG_FILE="$SCRIPT_DIR/logs/gc_server_$(date +%Y%m%d_%H%M%S).log"

echo -e "${GREEN}Starting server...${NC}"
echo "Logs will be written to: $LOG_FILE"
echo ""
echo -e "${YELLOW}Press Ctrl+C to stop the server${NC}"
echo ""

# Start the server with output to both terminal and log file
"$GC_SERVER" 2>&1 | tee "$LOG_FILE"

# Capture exit code
EXIT_CODE=${PIPESTATUS[0]}

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${GREEN}Server exited normally${NC}"
else
    echo -e "${RED}Server exited with error code: $EXIT_CODE${NC}"
fi

exit $EXIT_CODE
