#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lsattr.cpp lsattr_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[lsattr] Cleaning build artifacts..."
    rm -f lsattr.exe lsattr *.o *.obj
    echo "[lsattr] Clean complete."
    exit 0
fi

echo "[lsattr] Building lsattr..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lsattr] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lsattr] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:lsattr.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o lsattr.exe
fi

echo "[lsattr] Build successful: lsattr.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lsattr.exe "../../bin/lsattr.exe"
        echo "[lsattr] Installed to bin/lsattr.exe"
    fi
fi
