#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="ltrace.cpp ltrace_app.cpp engine.cpp options.cpp"

if [ "$1" = "clean" ]; then
    echo "[ltrace] Cleaning build artifacts..."
    rm -f ltrace.exe ltrace *.o *.obj
    echo "[ltrace] Clean complete."
    exit 0
fi

echo "[ltrace] Building ltrace..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[ltrace] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[ltrace] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES psapi.lib /Fe:ltrace.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lpsapi -o ltrace.exe
fi

echo "[ltrace] Build successful: ltrace.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp ltrace.exe "../../bin/ltrace.exe"
        echo "[ltrace] Installed to bin/ltrace.exe"
    fi
fi
