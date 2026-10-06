#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="pkill.cpp pkill_app.cpp engine.cpp options.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[pkill] Cleaning build artifacts..."
    rm -f pkill.exe pkill *.o *.obj
    echo "[pkill] Clean complete."
    exit 0
fi

echo "[pkill] Building pkill..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[pkill] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[pkill] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:pkill.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o pkill.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -Wl,/subsystem:console -o pkill.exe
fi

echo "[pkill] Build successful: pkill.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp pkill.exe "../../bin/pkill.exe"
        echo "[pkill] Installed to bin/pkill.exe"
    fi
fi
