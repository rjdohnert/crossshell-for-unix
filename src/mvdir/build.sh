#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="mvdir.cpp mvdir_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[mvdir] Cleaning build artifacts..."
    rm -f mvdir.exe mvdir *.o *.obj
    echo "[mvdir] Clean complete."
    exit 0
fi

echo "[mvdir] Building mvdir..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[mvdir] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[mvdir] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:mvdir.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES -o mvdir.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o mvdir.exe
fi

echo "[mvdir] Build successful: mvdir.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp mvdir.exe "../../bin/mvdir.exe"
        echo "[mvdir] Installed to bin/mvdir.exe"
    fi
fi
