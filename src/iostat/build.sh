#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f iostat iostat.exe *.o *.obj *.pdb *.ilk
    echo "[iostat] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[iostat] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[iostat] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 iostat.cpp -lpdh -Wl,/subsystem:console -o iostat.exe
    TARGET="iostat.exe"
else
    clang++ -std=c++17 -O2 iostat.cpp -lpdh -o iostat
    TARGET="iostat"
fi

echo "[iostat] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[iostat] Installed to ../../bin/$TARGET"
    fi
fi
