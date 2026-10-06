#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="netctl.cpp netctl_app.cpp engine.cpp options.cpp"
LIBS="-lws2_32 -liphlpapi -ladvapi32"

if [ "$1" = "clean" ]; then
    echo "[netctl] Cleaning build artifacts..."
    rm -f netctl.exe netctl *.o *.obj
    echo "[netctl] Clean complete."
    exit 0
fi

echo "[netctl] Building netctl..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[netctl] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[netctl] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES ws2_32.lib iphlpapi.lib advapi32.lib /Fe:netctl.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o netctl.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o netctl.exe
fi

echo "[netctl] Build successful: netctl.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp netctl.exe "../../bin/netctl.exe"
        echo "[netctl] Installed to bin/netctl.exe"
    fi
fi
