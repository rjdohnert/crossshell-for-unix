#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="arch.cpp arch_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[arch] Cleaning build artifacts..."
    rm -f arch.exe arch *.o *.obj
    echo "[arch] Clean complete."
    exit 0
fi

echo "[arch] Building arch..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[arch] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[arch] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:arch.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o arch.exe
fi

echo "[arch] Build successful: arch.exe"

if [ -d "../../bin" ]; then
    cp arch.exe "../../bin/arch.exe"
    echo "[arch] Installed to bin/arch.exe"
fi
