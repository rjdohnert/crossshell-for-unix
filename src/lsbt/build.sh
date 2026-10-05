#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lsbt.cpp lsbt_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lsbt] Cleaning build artifacts..."
    rm -f lsbt.exe lsbt *.o *.obj
    echo "[lsbt] Clean complete."
    exit 0
fi

echo "[lsbt] Building lsbt..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lsbt] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lsbt] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES bthprops.lib /Fe:lsbt.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lbthprops -o lsbt.exe
fi

echo "[lsbt] Build successful: lsbt.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lsbt.exe "../../bin/lsbt.exe"
        echo "[lsbt] Installed to bin/lsbt.exe"
    fi
fi
