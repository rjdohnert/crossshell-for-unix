#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="mkboot.cpp mkboot_app.cpp engine.cpp options.cpp"
LIBS="-lole32 -loleaut32 -lshlwapi -ladvapi32"

if [ "$1" = "clean" ]; then
    echo "[mkboot] Cleaning build artifacts..."
    rm -f mkboot.exe mkboot *.o *.obj
    echo "[mkboot] Clean complete."
    exit 0
fi

echo "[mkboot] Building mkboot..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[mkboot] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[mkboot] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES ole32.lib oleaut32.lib shlwapi.lib advapi32.lib /Fe:mkboot.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -luuid -o mkboot.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -o mkboot.exe
fi

echo "[mkboot] Build successful: mkboot.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp mkboot.exe "../../bin/mkboot.exe"
        echo "[mkboot] Installed to bin/mkboot.exe"
    fi
fi
