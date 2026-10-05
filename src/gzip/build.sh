#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f gzip gzip.exe *.o *.obj *.pdb *.ilk
    echo "[gzip] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[gzip] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[gzip] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 gzip.cpp -Wl,/subsystem:console -o gzip.exe
    TARGET="gzip.exe"
else
    clang++ -std=c++17 -O2 gzip.cpp -o gzip
    TARGET="gzip"
fi

echo "[gzip] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[gzip] Installed to ../../bin/$TARGET"
    fi
fi
