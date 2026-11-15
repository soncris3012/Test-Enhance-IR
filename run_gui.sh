#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

if [ ! -f "${BUILD_DIR}/ir_enhance_gui" ]; then
    echo "Executable not found. Building..."
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"
    cmake .. -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt;/opt/homebrew/opt/opencv" -DCMAKE_BUILD_TYPE=Release
    make -j4
fi

echo "Launching Infrared Image Detail Enhancement UI..."
"${BUILD_DIR}/ir_enhance_gui" &
