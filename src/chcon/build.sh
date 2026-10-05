#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

SOURCES="chcon.cpp chcon_app.cpp security.cpp options.cpp reporter.cpp"
TARGET="chcon"

if [ "${1:-}" = "clean" ]; then
    echo "[chcon] Cleaning build artifacts..."
    rm -f "${TARGET}" "${TARGET}.exe" *.o *.obj
    echo "[chcon] Clean complete."
    exit 0
fi

EXTRA_FLAGS=""
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    TARGET="chcon.exe"
    EXTRA_FLAGS="-municode -ladvapi32"
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[chcon] Error: Neither clang++ nor g++ was found in PATH." >&2
    exit 1
fi

echo "[chcon] Compiling with ${COMPILER}..."
${COMPILER} -std=c++17 -O2 ${SOURCES} ${EXTRA_FLAGS} -o "${TARGET}"

echo "[chcon] Build successful: ${TARGET}"

if [ -d "../../bin" ]; then
    cp -f "${TARGET}" "../../bin/${TARGET}"
    echo "[chcon] Installed to bin/${TARGET}"
fi
