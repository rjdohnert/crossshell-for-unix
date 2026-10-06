#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="nfsctl.cpp nfsctl_app.cpp controllers.cpp engine.cpp reporter.cpp options.cpp"
LIBS="-lmpr -lws2_32 -ladvapi32 -lnetapi32"

if [ "$1" = "clean" ]; then
    echo "[nfsctl] Cleaning build artifacts..."
    rm -f nfsctl.exe nfsctl *.o *.obj
    echo "[nfsctl] Clean complete."
    exit 0
fi

echo "[nfsctl] Building nfsctl..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[nfsctl] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[nfsctl] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES mpr.lib ws2_32.lib advapi32.lib netapi32.lib /Fe:nfsctl.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o nfsctl.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o nfsctl.exe
fi

echo "[nfsctl] Build successful: nfsctl.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp nfsctl.exe "../../bin/nfsctl.exe"
        echo "[nfsctl] Installed to bin/nfsctl.exe"
    fi
fi
