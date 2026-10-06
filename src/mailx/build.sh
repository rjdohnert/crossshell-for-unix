#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="mailx.cpp mailx_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[mailx] Cleaning build artifacts..."
    rm -f mailx.exe mailx *.o *.obj
    echo "[mailx] Clean complete."
    exit 0
fi

echo "[mailx] Building mailx..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[mailx] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[mailx] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:mailx.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES -o mailx.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o mailx.exe
fi

echo "[mailx] Build successful: mailx.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp mailx.exe "../../bin/mailx.exe"
        echo "[mailx] Installed to bin/mailx.exe"
    fi
fi
