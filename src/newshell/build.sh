#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="newshell.cpp newshell_app.cpp engine.cpp options.cpp"
LIBS="-lshell32"

if [ "$1" = "clean" ]; then
    echo "[newshell] Cleaning build artifacts..."
    rm -f newshell.exe newshell *.o *.obj
    echo "[newshell] Clean complete."
    exit 0
fi

echo "[newshell] Building newshell..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[newshell] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[newshell] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES shell32.lib /Fe:newshell.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o newshell.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o newshell.exe
fi

echo "[newshell] Build successful: newshell.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp newshell.exe "../../bin/newshell.exe"
        echo "[newshell] Installed to bin/newshell.exe"
    fi
fi
