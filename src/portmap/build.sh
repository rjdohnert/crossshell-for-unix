#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="portmap.cpp portmap_app.cpp engine.cpp options.cpp reporter.cpp"
LIBS="-lws2_32 -liphlpapi -lpsapi"

if [ "$1" = "clean" ]; then
    echo "[portmap] Cleaning build artifacts..."
    rm -f portmap.exe portmap *.o *.obj
    echo "[portmap] Clean complete."
    exit 0
fi

echo "[portmap] Building portmap..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[portmap] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[portmap] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES ws2_32.lib iphlpapi.lib psapi.lib /Fe:portmap.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 $SOURCES $LIBS -o portmap.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o portmap.exe
fi

echo "[portmap] Build successful: portmap.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp portmap.exe "../../bin/portmap.exe"
        echo "[portmap] Installed to bin/portmap.exe"
    fi
fi
