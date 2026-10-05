#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lsblk.cpp lsblk_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lsblk] Cleaning build artifacts..."
    rm -f lsblk.exe lsblk *.o *.obj
    echo "[lsblk] Clean complete."
    exit 0
fi

echo "[lsblk] Building lsblk..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lsblk] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lsblk] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES mpr.lib /Fe:lsblk.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lmpr -o lsblk.exe
fi

echo "[lsblk] Build successful: lsblk.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lsblk.exe "../../bin/lsblk.exe"
        echo "[lsblk] Installed to bin/lsblk.exe"
    fi
fi
