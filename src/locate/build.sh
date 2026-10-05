#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="locate.cpp locate_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[locate] Cleaning build artifacts..."
    rm -f locate.exe locate *.o *.obj
    echo "[locate] Clean complete."
    exit 0
fi

echo "[locate] Building locate..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[locate] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[locate] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:locate.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o locate.exe
fi

echo "[locate] Build successful: locate.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp locate.exe "../../bin/locate.exe"
        echo "[locate] Installed to bin/locate.exe"
    fi
fi
