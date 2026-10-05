#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="logger.cpp logger_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[logger] Cleaning build artifacts..."
    rm -f logger.exe logger *.o *.obj
    echo "[logger] Clean complete."
    exit 0
fi

echo "[logger] Building logger..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[logger] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[logger] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES Advapi32.lib /Fe:logger.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -ladvapi32 -o logger.exe
fi

echo "[logger] Build successful: logger.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp logger.exe "../../bin/logger.exe"
        echo "[logger] Installed to bin/logger.exe"
    fi
fi
