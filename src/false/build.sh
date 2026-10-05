#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f false false.exe *.o *.obj *.pdb *.ilk
    echo "[false] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[false] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[false] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 false.cpp -Wl,/subsystem:console -o false.exe
    TARGET="false.exe"
else
    clang++ -std=c++17 -O2 false.cpp -o false
    TARGET="false"
fi

echo "[false] Build successful: $TARGET"
if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
    fi
fi
