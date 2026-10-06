#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="pinky.cpp pinky_app.cpp engine.cpp options.cpp reporter.cpp"
LIBS="-lwtsapi32 -lnetapi32 -lws2_32"

if [ "$1" = "clean" ]; then
    echo "[pinky] Cleaning build artifacts..."
    rm -f pinky.exe pinky *.o *.obj
    echo "[pinky] Clean complete."
    exit 0
fi

echo "[pinky] Building pinky..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[pinky] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[pinky] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES wtsapi32.lib netapi32.lib ws2_32.lib /Fe:pinky.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES $LIBS -o pinky.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -Wl,/subsystem:console -o pinky.exe
fi

echo "[pinky] Build successful: pinky.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp pinky.exe "../../bin/pinky.exe"
        echo "[pinky] Installed to bin/pinky.exe"
    fi
fi
