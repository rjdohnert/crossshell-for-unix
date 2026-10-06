#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="patch.cpp patch_app.cpp engine.cpp options.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[patch] Cleaning build artifacts..."
    rm -f patch.exe patch *.o *.obj
    echo "[patch] Clean complete."
    exit 0
fi

echo "[patch] Building patch..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[patch] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[patch] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:patch.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 $SOURCES $LIBS -o patch.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o patch.exe
fi

echo "[patch] Build successful: patch.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp patch.exe "../../bin/patch.exe"
        echo "[patch] Installed to bin/patch.exe"
    fi
fi
