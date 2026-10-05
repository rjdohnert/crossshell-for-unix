#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lssrc.cpp lssrc_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lssrc] Cleaning build artifacts..."
    rm -f lssrc.exe lssrc *.o *.obj
    echo "[lssrc] Clean complete."
    exit 0
fi

echo "[lssrc] Building lssrc..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lssrc] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lssrc] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES advapi32.lib /Fe:lssrc.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -ladvapi32 -o lssrc.exe
fi

echo "[lssrc] Build successful: lssrc.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lssrc.exe "../../bin/lssrc.exe"
        echo "[lssrc] Installed to bin/lssrc.exe"
    fi
fi
