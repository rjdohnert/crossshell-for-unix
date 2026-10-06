#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="meminfo.cpp meminfo_app.cpp engine.cpp reporter.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[meminfo] Cleaning build artifacts..."
    rm -f meminfo.exe meminfo *.o *.obj
    echo "[meminfo] Clean complete."
    exit 0
fi

echo "[meminfo] Building meminfo..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[meminfo] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[meminfo] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES psapi.lib /Fe:meminfo.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES -lpsapi -o meminfo.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lpsapi -o meminfo.exe
fi

echo "[meminfo] Build successful: meminfo.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp meminfo.exe "../../bin/meminfo.exe"
        echo "[meminfo] Installed to bin/meminfo.exe"
    fi
fi
