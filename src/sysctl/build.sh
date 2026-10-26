#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="sysctl.cpp sysctl_app.cpp telemetry_provider.cpp storage_provider.cpp reporter.cpp options.cpp mib_tree.cpp common.cpp"
LIBS="-liphlpapi -ladvapi32 -lsetupapi -luuid -lpsapi"
MSVC_LIBS="iphlpapi.lib advapi32.lib setupapi.lib uuid.lib psapi.lib"

if [ "$1" = "clean" ]; then
    echo "[sysctl] Cleaning build artifacts..."
    rm -f sysctl.exe sysctl-tests.exe sysctl *.o *.obj *.pdb *.ilk
    echo "[sysctl] Clean complete."
    exit 0
fi

echo "[sysctl] Building sysctl..."

check_cmd() {
    which "$1" >NUL 2>&1 || which "$1" >/dev/null 2>&1 || command -v "$1" >NUL 2>&1 || command -v "$1" >/dev/null 2>&1
}

CXX_COMPILER=""
if check_cmd clang++; then
    CXX_COMPILER="clang++"
elif check_cmd g++; then
    CXX_COMPILER="g++"
elif check_cmd cl; then
    CXX_COMPILER="cl"
elif check_cmd cl.exe; then
    CXX_COMPILER="cl.exe"
else
    echo "[sysctl] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[sysctl] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ] || [ "$CXX_COMPILER" = "cl.exe" ]; then
    $CXX_COMPILER /nologo /EHsc /std:c++17 /O2 $SOURCES $MSVC_LIBS /Fe:sysctl.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES $LIBS -Wl,/subsystem:console -o sysctl.exe
fi

echo "[sysctl] Build successful: sysctl.exe"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp sysctl.exe "../../bin/sysctl.exe"
        echo "[sysctl] Installed to bin/sysctl.exe"
    elif [ -d "../bin" ]; then
        cp sysctl.exe "../bin/sysctl.exe"
        echo "[sysctl] Installed to bin/sysctl.exe"
    fi
fi
