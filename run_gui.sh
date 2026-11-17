#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

if [ ! -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release)
    if [ -z "${CMAKE_PREFIX_PATH:-}" ] && command -v brew >/dev/null 2>&1; then
        QT_PREFIX="$(brew --prefix qt 2>/dev/null || true)"
        CV_PREFIX="$(brew --prefix opencv 2>/dev/null || true)"
        PREFIX_LIST=""
        [ -n "${QT_PREFIX}" ] && PREFIX_LIST="${QT_PREFIX}"
        [ -n "${CV_PREFIX}" ] && PREFIX_LIST="${PREFIX_LIST:+${PREFIX_LIST};}${CV_PREFIX}"
        if [ -n "${PREFIX_LIST}" ]; then
            CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=${PREFIX_LIST}")
        fi
    fi
    cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" "${CMAKE_ARGS[@]}"
fi

JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
cmake --build "${BUILD_DIR}" --config Release -j"${JOBS}"

echo "Launching Infrared Image Detail Enhancement UI..."
"${BUILD_DIR}/ir_enhance_gui" "$@" &
