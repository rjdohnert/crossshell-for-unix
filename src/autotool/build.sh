#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="autoconf.cpp automake.cpp autotool.cpp autotool_app.cpp build_engine.cpp help.cpp libtool.cpp port_engine.cpp utils.cpp"

if [ "$1" = "clean" ]; then
    echo "[autotool] Cleaning build artifacts..."
    rm -f autotool.exe autotool *.o *.obj
    echo "[autotool] Clean complete."
    exit 0
fi

echo "[autotool] Building autotool..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[autotool] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[autotool] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:autotool.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o autotool.exe
fi

echo "[autotool] Build successful: autotool.exe"

if [ -d "../../bin" ]; then
    cp autotool.exe "../../bin/autotool.exe"
    echo "[autotool] Installed to bin/autotool.exe"
fi
