#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="logout.cpp logout_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[logout] Cleaning build artifacts..."
    rm -f logout.exe logout *.o *.obj
    echo "[logout] Clean complete."
    exit 0
fi

echo "[logout] Building logout..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[logout] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[logout] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES user32.lib /Fe:logout.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -luser32 -o logout.exe
fi

echo "[logout] Build successful: logout.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp logout.exe "../../bin/logout.exe"
        echo "[logout] Installed to bin/logout.exe"
    fi
fi
