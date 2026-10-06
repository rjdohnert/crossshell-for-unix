#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="pbcopy.cpp pbcopy_app.cpp engine.cpp options.cpp"
LIBS="-luser32"

if [ "$1" = "clean" ]; then
    echo "[pbcopy] Cleaning build artifacts..."
    rm -f pbcopy.exe pbcopy *.o *.obj
    echo "[pbcopy] Clean complete."
    exit 0
fi

echo "[pbcopy] Building pbcopy..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[pbcopy] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[pbcopy] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES user32.lib /Fe:pbcopy.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o pbcopy.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -Wl,/subsystem:console -o pbcopy.exe
fi

echo "[pbcopy] Build successful: pbcopy.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp pbcopy.exe "../../bin/pbcopy.exe"
        echo "[pbcopy] Installed to bin/pbcopy.exe"
    fi
fi
