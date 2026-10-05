#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lsuser.cpp lsuser_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lsuser] Cleaning build artifacts..."
    rm -f lsuser.exe lsuser *.o *.obj
    echo "[lsuser] Clean complete."
    exit 0
fi

echo "[lsuser] Building lsuser..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lsuser] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lsuser] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES Netapi32.lib Advapi32.lib Secur32.lib /Fe:lsuser.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lnetapi32 -ladvapi32 -lsecur32 -o lsuser.exe
fi

echo "[lsuser] Build successful: lsuser.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lsuser.exe "../../bin/lsuser.exe"
        echo "[lsuser] Installed to bin/lsuser.exe"
    fi
fi
