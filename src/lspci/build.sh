#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lspci.cpp lspci_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lspci] Cleaning build artifacts..."
    rm -f lspci.exe lspci *.o *.obj
    echo "[lspci] Clean complete."
    exit 0
fi

echo "[lspci] Building lspci..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lspci] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lspci] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES setupapi.lib /Fe:lspci.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lsetupapi -o lspci.exe
fi

echo "[lspci] Build successful: lspci.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lspci.exe "../../bin/lspci.exe"
        echo "[lspci] Installed to bin/lspci.exe"
    fi
fi
