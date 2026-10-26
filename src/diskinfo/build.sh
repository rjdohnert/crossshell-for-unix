#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="diskinfo.cpp diskinfo_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[diskinfo] Cleaning build artifacts..."
    rm -f diskinfo.exe *.o *.obj
    echo "[diskinfo] Clean complete."
    exit 0
fi

echo "[diskinfo] Building diskinfo..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[diskinfo] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[diskinfo] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES mpr.lib /Fe:diskinfo.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lmpr -o diskinfo.exe
fi

echo "[diskinfo] Build successful: diskinfo.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp diskinfo.exe "../../bin/diskinfo.exe"
        echo "[diskinfo] Installed to bin/diskinfo.exe"
    fi
fi
