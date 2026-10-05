#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lscfg.cpp lscfg_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lscfg] Cleaning build artifacts..."
    rm -f lscfg.exe lscfg *.o *.obj
    echo "[lscfg] Clean complete."
    exit 0
fi

echo "[lscfg] Building lscfg..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lscfg] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lscfg] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES wbemuuid.lib ole32.lib oleaut32.lib /Fe:lscfg.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lwbemuuid -lole32 -loleaut32 -o lscfg.exe
fi

echo "[lscfg] Build successful: lscfg.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lscfg.exe "../../bin/lscfg.exe"
        echo "[lscfg] Installed to bin/lscfg.exe"
    fi
fi
