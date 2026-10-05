#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

SOURCES="bat.cpp bat_app.cpp engine.cpp highlighter.cpp options.cpp reporter.cpp"
TARGET="bat"

if [ "${1:-}" = "clean" ]; then
    echo "[bat] Cleaning build artifacts..."
    rm -f "${TARGET}" "${TARGET}.exe" *.o *.obj
    echo "[bat] Clean complete."
    exit 0
fi

if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    TARGET="bat.exe"
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[bat] Error: Neither clang++ nor g++ was found in PATH." >&2
    exit 1
fi

echo "[bat] Compiling with ${COMPILER}..."
${COMPILER} -std=c++17 -O2 ${SOURCES} -o "${TARGET}"

echo "[bat] Build successful: ${TARGET}"

if [ -d "../../bin" ]; then
    cp -f "${TARGET}" "../../bin/${TARGET}"
    echo "[bat] Installed to bin/${TARGET}"
fi
