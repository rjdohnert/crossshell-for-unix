#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="pgrep.cpp pgrep_app.cpp engine.cpp options.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[pgrep] Cleaning build artifacts..."
    rm -f pgrep.exe pgrep *.o *.obj
    echo "[pgrep] Clean complete."
    exit 0
fi

echo "[pgrep] Building pgrep..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[pgrep] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[pgrep] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:pgrep.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o pgrep.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -Wl,/subsystem:console -o pgrep.exe
fi

echo "[pgrep] Build successful: pgrep.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp pgrep.exe "../../bin/pgrep.exe"
        echo "[pgrep] Installed to bin/pgrep.exe"
    fi
fi
