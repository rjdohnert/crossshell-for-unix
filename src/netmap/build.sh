#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="netmap.cpp netmap_app.cpp engine.cpp options.cpp"
LIBS="-liphlpapi -lws2_32 -ladvapi32"

if [ "$1" = "clean" ]; then
    echo "[netmap] Cleaning build artifacts..."
    rm -f netmap.exe netmap *.o *.obj
    echo "[netmap] Clean complete."
    exit 0
fi

echo "[netmap] Building netmap..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[netmap] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[netmap] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES iphlpapi.lib ws2_32.lib advapi32.lib /Fe:netmap.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o netmap.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o netmap.exe
fi

echo "[netmap] Build successful: netmap.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp netmap.exe "../../bin/netmap.exe"
        echo "[netmap] Installed to bin/netmap.exe"
    fi
fi
