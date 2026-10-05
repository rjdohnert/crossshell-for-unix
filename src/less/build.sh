#!/usr/bin/env bash
set -e

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f less less.exe *.o *.obj *.pdb *.ilk
    echo "[less] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[less] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[less] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 less.cpp -Wl,/subsystem:console -o less.exe
    TARGET="less.exe"
else
    clang++ -std=c++17 -O2 less.cpp -o less
    TARGET="less"
fi

echo "[less] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[less] Installed to ../../bin/$TARGET"
    fi
fi
