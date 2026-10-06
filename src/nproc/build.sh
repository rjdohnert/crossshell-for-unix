#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="nproc.cpp nproc_app.cpp engine.cpp options.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[nproc] Cleaning build artifacts..."
    rm -f nproc.exe nproc *.o *.obj
    echo "[nproc] Clean complete."
    exit 0
fi

echo "[nproc] Building nproc..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[nproc] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[nproc] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:nproc.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 $SOURCES $LIBS -o nproc.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o nproc.exe
fi

echo "[nproc] Build successful: nproc.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp nproc.exe "../../bin/nproc.exe"
        echo "[nproc] Installed to bin/nproc.exe"
    fi
fi
