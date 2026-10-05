#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="logname.cpp logname_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[logname] Cleaning build artifacts..."
    rm -f logname.exe logname *.o *.obj
    echo "[logname] Clean complete."
    exit 0
fi

echo "[logname] Building logname..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[logname] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[logname] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES advapi32.lib /Fe:logname.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -ladvapi32 -o logname.exe
fi

echo "[logname] Build successful: logname.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp logname.exe "../../bin/logname.exe"
        echo "[logname] Installed to bin/logname.exe"
    fi
fi
