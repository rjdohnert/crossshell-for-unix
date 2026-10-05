#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lswifi.cpp lswifi_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lswifi] Cleaning build artifacts..."
    rm -f lswifi.exe lswifi *.o *.obj
    echo "[lswifi] Clean complete."
    exit 0
fi

echo "[lswifi] Building lswifi..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lswifi] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lswifi] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES wlanapi.lib ole32.lib /Fe:lswifi.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lwlanapi -lole32 -o lswifi.exe
fi

echo "[lswifi] Build successful: lswifi.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lswifi.exe "../../bin/lswifi.exe"
        echo "[lswifi] Installed to bin/lswifi.exe"
    fi
fi
