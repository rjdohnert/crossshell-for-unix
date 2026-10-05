#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

SOURCES="chroot.cpp chroot_app.cpp path_helper.cpp options.cpp executor.cpp"
TARGET="chroot"

if [ "${1:-}" = "clean" ]; then
    echo "[chroot] Cleaning build artifacts..."
    rm -f "${TARGET}" "${TARGET}.exe" *.o *.obj
    echo "[chroot] Clean complete."
    exit 0
fi

EXTRA_FLAGS=""
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    TARGET="chroot.exe"
    EXTRA_FLAGS="-municode"
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[chroot] Error: Neither clang++ nor g++ was found in PATH." >&2
    exit 1
fi

echo "[chroot] Compiling with ${COMPILER}..."
${COMPILER} -std=c++17 -O2 ${SOURCES} ${EXTRA_FLAGS} -o "${TARGET}"

echo "[chroot] Build successful: ${TARGET}"

if [ -d "../../bin" ]; then
    cp -f "${TARGET}" "../../bin/${TARGET}"
    echo "[chroot] Installed to bin/${TARGET}"
fi
