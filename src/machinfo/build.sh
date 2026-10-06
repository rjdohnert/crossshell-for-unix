#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="machinfo.cpp machinfo_app.cpp engine.cpp options.cpp reporter.cpp"

if [ "$1" = "clean" ]; then
    echo "[machinfo] Cleaning build artifacts..."
    rm -f machinfo.exe machinfo *.o *.obj
    echo "[machinfo] Clean complete."
    exit 0
fi

echo "[machinfo] Building machinfo..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[machinfo] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[machinfo] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES wbemuuid.lib ole32.lib oleaut32.lib tbs.lib advapi32.lib /Fe:machinfo.exe
    rm -f *.obj
elif [ "$CXX_COMPILER" = "g++" ]; then
    g++ -std=c++17 -O2 -municode $SOURCES -lwbemuuid -lole32 -loleaut32 -ltbs -ladvapi32 -o machinfo.exe
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -lwbemuuid -lole32 -loleaut32 -ltbs -ladvapi32 -o machinfo.exe
fi

echo "[machinfo] Build successful: machinfo.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp machinfo.exe "../../bin/machinfo.exe"
        echo "[machinfo] Installed to bin/machinfo.exe"
    fi
fi
