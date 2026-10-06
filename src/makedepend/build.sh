#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="makedepend.cpp makedepend_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[makedepend] Cleaning build artifacts..."
    rm -f makedepend.exe makedepend *.o *.obj
    echo "[makedepend] Clean complete."
    exit 0
fi

echo "[makedepend] Building makedepend..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[makedepend] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[makedepend] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:makedepend.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES -o makedepend.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o makedepend.exe
fi

echo "[makedepend] Build successful: makedepend.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp makedepend.exe "../../bin/makedepend.exe"
        echo "[makedepend] Installed to bin/makedepend.exe"
    fi
fi
