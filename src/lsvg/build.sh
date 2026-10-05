#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lsvg.cpp lsvg_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lsvg] Cleaning build artifacts..."
    rm -f lsvg.exe lsvg *.o *.obj
    echo "[lsvg] Clean complete."
    exit 0
fi

echo "[lsvg] Building lsvg..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lsvg] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lsvg] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:lsvg.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o lsvg.exe
fi

echo "[lsvg] Build successful: lsvg.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lsvg.exe "../../bin/lsvg.exe"
        echo "[lsvg] Installed to bin/lsvg.exe"
    fi
fi
