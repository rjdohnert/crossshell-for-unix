#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="nice.cpp nice_app.cpp engine.cpp options.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[nice] Cleaning build artifacts..."
    rm -f nice.exe nice *.o *.obj
    echo "[nice] Clean complete."
    exit 0
fi

echo "[nice] Building nice..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[nice] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[nice] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:nice.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o nice.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o nice.exe
fi

echo "[nice] Build successful: nice.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp nice.exe "../../bin/nice.exe"
        echo "[nice] Installed to bin/nice.exe"
    fi
fi
