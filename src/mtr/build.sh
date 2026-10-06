#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="mtr.cpp mtr_app.cpp engine.cpp reporter.cpp options.cpp"
LIBS="-lws2_32 -liphlpapi"

if [ "$1" = "clean" ]; then
    echo "[mtr] Cleaning build artifacts..."
    rm -f mtr.exe mtr *.o *.obj
    echo "[mtr] Clean complete."
    exit 0
fi

echo "[mtr] Building mtr..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[mtr] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[mtr] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES ws2_32.lib iphlpapi.lib /Fe:mtr.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o mtr.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o mtr.exe
fi

echo "[mtr] Build successful: mtr.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp mtr.exe "../../bin/mtr.exe"
        echo "[mtr] Installed to bin/mtr.exe"
    fi
fi
