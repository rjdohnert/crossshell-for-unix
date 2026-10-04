#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

SOURCES="awk.cpp awk_app.cpp context.cpp evaluator.cpp parser.cpp string_utils.cpp value.cpp win_loader.cpp"
TARGET="awk"

if [ "${1:-}" = "clean" ]; then
    echo "[awk] Cleaning build artifacts..."
    rm -f "${TARGET}" "${TARGET}.exe" *.o *.obj
    echo "[awk] Clean complete."
    exit 0
fi

LIBS=""
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    LIBS="-lwbemuuid -lole32 -loleaut32 -ladvapi32"
    TARGET="awk.exe"
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[awk] Error: Neither clang++ nor g++ was found in PATH." >&2
    exit 1
fi

echo "[awk] Compiling with ${COMPILER}..."
${COMPILER} -std=c++17 -O2 ${SOURCES} ${LIBS} -o "${TARGET}"

echo "[awk] Build successful: ${TARGET}"

if [ -d "../../bin" ]; then
    cp -f "${TARGET}" "../../bin/${TARGET}"
    echo "[awk] Installed to bin/${TARGET}"
fi
