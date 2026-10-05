#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

SOURCES="bc.cpp bc_app.cpp engine.cpp options.cpp parser.cpp reporter.cpp"
TARGET="bc"

if [ "${1:-}" = "clean" ]; then
    echo "[bc] Cleaning build artifacts..."
    rm -f "${TARGET}" "${TARGET}.exe" *.o *.obj
    echo "[bc] Clean complete."
    exit 0
fi

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    TARGET="bc.exe"
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[bc] Error: Neither clang++ nor g++ was found in PATH." >&2
    exit 1
fi

echo "[bc] Compiling with ${COMPILER}..."
${COMPILER} -std=c++17 -O2 ${SOURCES} -o "${TARGET}"

echo "[bc] Build successful: ${TARGET}"

if [ -d "../../bin" ]; then
    cp -f "${TARGET}" "../../bin/${TARGET}"
    echo "[bc] Installed to bin/${TARGET}"
fi
