#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f fold fold.exe *.o *.obj *.pdb *.ilk
    echo "[fold] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[fold] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[fold] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 fold.cpp -Wl,/subsystem:console -o fold.exe
    TARGET="fold.exe"
else
    clang++ -std=c++17 -O2 fold.cpp -o fold
    TARGET="fold"
fi

echo "[fold] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[fold] Installed to ../../bin/$TARGET"
    fi
fi
