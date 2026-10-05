#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lsdev.cpp lsdev_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lsdev] Cleaning build artifacts..."
    rm -f lsdev.exe lsdev *.o *.obj
    echo "[lsdev] Clean complete."
    exit 0
fi

echo "[lsdev] Building lsdev..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lsdev] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lsdev] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES setupapi.lib cfgmgr32.lib /Fe:lsdev.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lsetupapi -lcfgmgr32 -o lsdev.exe
fi

echo "[lsdev] Build successful: lsdev.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lsdev.exe "../../bin/lsdev.exe"
        echo "[lsdev] Installed to bin/lsdev.exe"
    fi
fi
