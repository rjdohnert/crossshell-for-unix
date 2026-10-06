#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="ninja.cpp ninja_app.cpp engine.cpp options.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[ninja] Cleaning build artifacts..."
    rm -f ninja.exe ninja *.o *.obj
    echo "[ninja] Clean complete."
    exit 0
fi

echo "[ninja] Building ninja..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[ninja] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[ninja] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:ninja.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o ninja.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o ninja.exe
fi

echo "[ninja] Build successful: ninja.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp ninja.exe "../../bin/ninja.exe"
        echo "[ninja] Installed to bin/ninja.exe"
    fi
fi
