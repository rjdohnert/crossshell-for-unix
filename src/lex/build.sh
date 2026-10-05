#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="lex.cpp lex_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[lex] Cleaning build artifacts..."
    rm -f lex.exe lex *.o *.obj
    echo "[lex] Clean complete."
    exit 0
fi

echo "[lex] Building lex..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[lex] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[lex] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES /Fe:lex.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES -o lex.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -o lex.exe
fi

echo "[lex] Build successful: lex.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp lex.exe "../../bin/lex.exe"
        echo "[lex] Installed to bin/lex.exe"
    fi
fi
