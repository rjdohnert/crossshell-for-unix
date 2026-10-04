#!/bin/sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SOURCES="auditctl.cpp auditctl_app.cpp audit_policy.cpp audit_session.cpp audit_types.cpp cli.cpp"

if [ "$1" = "clean" ]; then
    echo "[auditctl] Cleaning build artifacts..."
    rm -f auditctl.exe auditctl *.o *.obj
    echo "[auditctl] Clean complete."
    exit 0
fi

echo "[auditctl] Building auditctl..."

CXX_COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    CXX_COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    CXX_COMPILER="g++"
elif command -v cl >/dev/null 2>&1; then
    CXX_COMPILER="cl"
else
    echo "[auditctl] Error: No C++ compiler found (clang++, g++, cl)"
    exit 1
fi

echo "[auditctl] Compiling with $CXX_COMPILER..."
if [ "$CXX_COMPILER" = "cl" ]; then
    cl /nologo /EHsc /std:c++17 /O2 $SOURCES Advapi32.lib /Fe:auditctl.exe
    rm -f *.obj
else
    $CXX_COMPILER -std=c++17 -O2 $SOURCES -ladvapi32 -o auditctl.exe
fi

echo "[auditctl] Build successful: auditctl.exe"

if [ -d "../../bin" ]; then
    cp auditctl.exe "../../bin/auditctl.exe"
    echo "[auditctl] Installed to bin/auditctl.exe"
fi
