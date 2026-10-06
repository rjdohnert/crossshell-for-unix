#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="mv.cpp mv_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[mv] Cleaning build artifacts..."
    rm -f mv.exe mv *.o *.obj
    echo "[mv] Clean complete."
    exit 0
fi

echo "[mv] Building mv..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[mv] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[mv] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:mv.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES -o mv.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o mv.exe
fi

echo "[mv] Build successful: mv.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp mv.exe "../../bin/mv.exe"
        echo "[mv] Installed to bin/mv.exe"
    fi
fi
