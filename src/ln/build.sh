#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="ln.cpp ln_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[ln] Cleaning build artifacts..."
    rm -f ln.exe ln *.o *.obj
    echo "[ln] Clean complete."
    exit 0
fi

echo "[ln] Building ln..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[ln] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[ln] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES advapi32.lib /Fe:ln.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -ladvapi32 -o ln.exe
fi

echo "[ln] Build successful: ln.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp ln.exe "../../bin/ln.exe"
        echo "[ln] Installed to bin/ln.exe"
    fi
fi
