#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f head head.exe *.o *.obj *.pdb *.ilk
    echo "[head] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[head] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[head] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 head.cpp -Wl,/subsystem:console -o head.exe
    TARGET="head.exe"
else
    clang++ -std=c++17 -O2 head.cpp -o head
    TARGET="head"
fi

echo "[head] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[head] Installed to ../../bin/$TARGET"
    fi
fi
