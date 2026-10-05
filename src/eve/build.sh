#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f eve eve.exe *.o *.obj *.pdb *.ilk
    echo "[eve] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[eve] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[eve] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 eve.cpp -Wl,/subsystem:console -o eve.exe
    TARGET="eve.exe"
else
    clang++ -std=c++17 -O2 eve.cpp -o eve
    TARGET="eve"
fi

echo "[eve] Build successful: $TARGET"
if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
    fi
fi
