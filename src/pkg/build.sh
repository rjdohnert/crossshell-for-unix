#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="pkg.cpp pkg_app.cpp engine.cpp options.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[pkg] Cleaning build artifacts..."
    rm -f pkg.exe pkg *.o *.obj
    echo "[pkg] Clean complete."
    exit 0
fi

echo "[pkg] Building pkg..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[pkg] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[pkg] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:pkg.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o pkg.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -Wl,/subsystem:console -o pkg.exe
fi

echo "[pkg] Build successful: pkg.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp pkg.exe "../../bin/pkg.exe"
        echo "[pkg] Installed to bin/pkg.exe"
    fi
fi
