#!/usr/bin/env bash
set -e

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f last last.exe *.o *.obj *.pdb *.ilk
    echo "[last] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[last] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[last] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 last.cpp -lwevtapi -ladvapi32 -Wl,/subsystem:console -o last.exe
    TARGET="last.exe"
else
    clang++ -std=c++17 -O2 last.cpp -lwevtapi -ladvapi32 -o last
    TARGET="last"
fi

echo "[last] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[last] Installed to ../../bin/$TARGET"
    fi
fi
