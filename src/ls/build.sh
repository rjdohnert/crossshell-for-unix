#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="ls.cpp ls_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[ls] Cleaning build artifacts..."
    rm -f ls.exe ls *.o *.obj
    echo "[ls] Clean complete."
    exit 0
fi

echo "[ls] Building ls..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[ls] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[ls] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES advapi32.lib /Fe:ls.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -ladvapi32 -o ls.exe
fi

echo "[ls] Build successful: ls.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp ls.exe "../../bin/ls.exe"
        echo "[ls] Installed to bin/ls.exe"
    fi
fi
