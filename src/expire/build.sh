#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f expire expire.exe *.o *.obj *.pdb *.ilk
    echo "[expire] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[expire] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[expire] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 expire.cpp -Wl,/subsystem:console -o expire.exe
    TARGET="expire.exe"
else
    clang++ -std=c++17 -O2 expire.cpp -o expire
    TARGET="expire"
fi

echo "[expire] Build successful: $TARGET"
if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
    fi
fi
