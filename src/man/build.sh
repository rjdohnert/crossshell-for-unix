#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="man.cpp man_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[man] Cleaning build artifacts..."
    rm -f man.exe man *.o *.obj
    echo "[man] Clean complete."
    exit 0
fi

echo "[man] Building man..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[man] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[man] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:man.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES -o man.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o man.exe
fi

echo "[man] Build successful: man.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp man.exe "../../bin/man.exe"
        echo "[man] Installed to bin/man.exe"
    fi
fi
