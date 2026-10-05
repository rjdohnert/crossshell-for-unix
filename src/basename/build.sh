#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

SOURCES="basename.cpp basename_app.cpp engine.cpp options.cpp reporter.cpp"
TARGET="basename"

if [ "${1:-}" = "clean" ]; then
    echo "[basename] Cleaning build artifacts..."
    rm -f "${TARGET}" "${TARGET}.exe" *.o *.obj
    echo "[basename] Clean complete."
    exit 0
fi

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    TARGET="basename.exe"
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[basename] Error: Neither clang++ nor g++ was found in PATH." >&2
    exit 1
fi

echo "[basename] Compiling with ${COMPILER}..."
${COMPILER} -std=c++17 -O2 ${SOURCES} -o "${TARGET}"

echo "[basename] Build successful: ${TARGET}"

if [ -d "../../bin" ]; then
    cp -f "${TARGET}" "../../bin/${TARGET}"
    echo "[basename] Installed to bin/${TARGET}"
fi
