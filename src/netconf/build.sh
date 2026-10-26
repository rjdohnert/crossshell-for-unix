#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

SOURCES="netconf.c"
LIBS="-liphlpapi -lws2_32"
TARGET="netconf.exe"

if [ "${1:-}" = "clean" ]; then
    echo "[netconf] Cleaning build artifacts..."
    rm -f "${TARGET}" netconf *.o *.obj *.pdb *.ilk
    echo "[netconf] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang >/dev/null 2>&1; then
    COMPILER="clang"
elif command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v gcc >/dev/null 2>&1; then
    COMPILER="gcc"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    COMPILER="cl"
else
    echo "[netconf] Error: No supported compiler found (clang, clang++, gcc, g++, cl)." >&2
    exit 1
fi

echo "[netconf] Compiling with ${COMPILER}..."
if [ "${COMPILER}" = "cl" ]; then
    cl /nologo /O2 ${SOURCES} iphlpapi.lib ws2_32.lib /Fe:"${TARGET}"
    rm -f *.obj
else
    ${COMPILER} -O2 ${SOURCES} ${LIBS} -o "${TARGET}"
fi

echo "[netconf] Build successful: ${TARGET}"

if [ "${1:-}" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "${TARGET}" "../../bin/${TARGET}"
        echo "[netconf] Installed to bin/${TARGET}"
    fi
fi
