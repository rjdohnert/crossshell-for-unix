#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="nohup.cpp nohup_app.cpp engine.cpp options.cpp"
LIBS=""

if [ "$1" = "clean" ]; then
    echo "[nohup] Cleaning build artifacts..."
    rm -f nohup.exe nohup *.o *.obj
    echo "[nohup] Clean complete."
    exit 0
fi

echo "[nohup] Building nohup..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[nohup] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[nohup] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:nohup.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o nohup.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -Wl,/subsystem:console -o nohup.exe
fi

echo "[nohup] Build successful: nohup.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp nohup.exe "../../bin/nohup.exe"
        echo "[nohup] Installed to bin/nohup.exe"
    fi
fi
