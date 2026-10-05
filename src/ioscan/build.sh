#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f ioscan ioscan.exe *.o *.obj *.pdb *.ilk
    echo "[ioscan] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[ioscan] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[ioscan] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 ioscan.cpp -lsetupapi -lcfgmgr32 -Wl,/subsystem:console -o ioscan.exe
    TARGET="ioscan.exe"
else
    clang++ -std=c++17 -O2 ioscan.cpp -lsetupapi -lcfgmgr32 -o ioscan
    TARGET="ioscan"
fi

echo "[ioscan] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[ioscan] Installed to ../../bin/$TARGET"
    fi
fi
