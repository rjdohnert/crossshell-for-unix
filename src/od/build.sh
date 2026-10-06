#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="od.cpp od_app.cpp engine.cpp options.cpp reporter.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[od] Cleaning build artifacts..."
    rm -f od.exe od *.o *.obj
    echo "[od] Clean complete."
    exit 0
fi

echo "[od] Building od..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[od] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[od] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:od.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 $SOURCES $LIBS -o od.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o od.exe
fi

echo "[od] Build successful: od.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp od.exe "../../bin/od.exe"
        echo "[od] Installed to bin/od.exe"
    fi
fi
