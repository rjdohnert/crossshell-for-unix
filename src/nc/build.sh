#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="nc.cpp nc_app.cpp engine.cpp options.cpp"
LIBS="-lws2_32"

if [ "$1" = "clean" ]; then
    echo "[nc] Cleaning build artifacts..."
    rm -f nc.exe nc *.o *.obj
    echo "[nc] Clean complete."
    exit 0
fi

echo "[nc] Building nc..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[nc] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[nc] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES ws2_32.lib /Fe:nc.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o nc.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o nc.exe
fi

echo "[nc] Build successful: nc.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp nc.exe "../../bin/nc.exe"
        echo "[nc] Installed to bin/nc.exe"
    fi
fi
