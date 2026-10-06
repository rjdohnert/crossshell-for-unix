#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="make.cpp make_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[make] Cleaning build artifacts..."
    rm -f make.exe make *.o *.obj
    echo "[make] Clean complete."
    exit 0
fi

echo "[make] Building make..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[make] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[make] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:make.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES -o make.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o make.exe
fi

echo "[make] Build successful: make.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp make.exe "../../bin/make.exe"
        echo "[make] Installed to bin/make.exe"
    fi
fi
