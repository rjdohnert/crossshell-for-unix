#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="pbpaste.cpp pbpaste_app.cpp engine.cpp options.cpp"
LIBS="-luser32 -lshell32"

if [ "$1" = "clean" ]; then
    echo "[pbpaste] Cleaning build artifacts..."
    rm -f pbpaste.exe pbpaste *.o *.obj
    echo "[pbpaste] Clean complete."
    exit 0
fi

echo "[pbpaste] Building pbpaste..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[pbpaste] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[pbpaste] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES user32.lib shell32.lib /Fe:pbpaste.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 $SOURCES $LIBS -o pbpaste.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o pbpaste.exe
fi

echo "[pbpaste] Build successful: pbpaste.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp pbpaste.exe "../../bin/pbpaste.exe"
        echo "[pbpaste] Installed to bin/pbpaste.exe"
    fi
fi
