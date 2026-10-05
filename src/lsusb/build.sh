#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lsusb.cpp lsusb_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lsusb] Cleaning build artifacts..."
    rm -f lsusb.exe lsusb *.o *.obj
    echo "[lsusb] Clean complete."
    exit 0
fi

echo "[lsusb] Building lsusb..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lsusb] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lsusb] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES setupapi.lib /Fe:lsusb.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lsetupapi -o lsusb.exe
fi

echo "[lsusb] Build successful: lsusb.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lsusb.exe "../../bin/lsusb.exe"
        echo "[lsusb] Installed to bin/lsusb.exe"
    fi
fi
