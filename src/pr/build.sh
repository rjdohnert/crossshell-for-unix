#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="pr.cpp pr_app.cpp engine.cpp options.cpp reporter.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[pr] Cleaning build artifacts..."
    rm -f pr.exe pr *.o *.obj
    echo "[pr] Clean complete."
    exit 0
fi

echo "[pr] Building pr..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[pr] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[pr] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:pr.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o pr.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -Wl,/subsystem:console -o pr.exe
fi

echo "[pr] Build successful: pr.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp pr.exe "../../bin/pr.exe"
        echo "[pr] Installed to bin/pr.exe"
    fi
fi
