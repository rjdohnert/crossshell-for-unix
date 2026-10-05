#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f id id.exe *.o *.obj *.pdb *.ilk
    echo "[id] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[id] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[id] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 id.cpp -ladvapi32 -lnetapi32 -Wl,/subsystem:console -o id.exe
    TARGET="id.exe"
else
    clang++ -std=c++17 -O2 id.cpp -ladvapi32 -lnetapi32 -o id
    TARGET="id"
fi

echo "[id] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[id] Installed to ../../bin/$TARGET"
    fi
fi
