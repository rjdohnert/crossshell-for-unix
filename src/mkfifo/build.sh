#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="mkfifo.cpp mkfifo_app.cpp engine.cpp options.cpp"
LIBS="-ladvapi32"

if [ "$1" = "clean" ]; then
    echo "[mkfifo] Cleaning build artifacts..."
    rm -f mkfifo.exe mkfifo *.o *.obj
    echo "[mkfifo] Clean complete."
    exit 0
fi

echo "[mkfifo] Building mkfifo..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[mkfifo] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[mkfifo] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES advapi32.lib /Fe:mkfifo.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o mkfifo.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o mkfifo.exe
fi

echo "[mkfifo] Build successful: mkfifo.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp mkfifo.exe "../../bin/mkfifo.exe"
        echo "[mkfifo] Installed to bin/mkfifo.exe"
    fi
fi
