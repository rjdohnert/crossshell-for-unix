#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="mount.cpp mount_app.cpp engine.cpp options.cpp"
LIBS="-lmpr -lvirtdisk -ladvapi32"

if [ "$1" = "clean" ]; then
    echo "[mount] Cleaning build artifacts..."
    rm -f mount.exe mount *.o *.obj
    echo "[mount] Clean complete."
    exit 0
fi

echo "[mount] Building mount..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[mount] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[mount] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES mpr.lib virtdisk.lib advapi32.lib /Fe:mount.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o mount.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o mount.exe
fi

echo "[mount] Build successful: mount.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp mount.exe "../../bin/mount.exe"
        echo "[mount] Installed to bin/mount.exe"
    fi
fi
