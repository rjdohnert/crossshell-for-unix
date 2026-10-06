#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="pax.cpp pax_app.cpp engine.cpp options.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[pax] Cleaning build artifacts..."
    rm -f pax.exe pax *.o *.obj
    echo "[pax] Clean complete."
    exit 0
fi

echo "[pax] Building pax..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[pax] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[pax] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:pax.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 $SOURCES $LIBS -o pax.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o pax.exe
fi

echo "[pax] Build successful: pax.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp pax.exe "../../bin/pax.exe"
        echo "[pax] Installed to bin/pax.exe"
    fi
fi
