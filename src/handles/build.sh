#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f handles handles.exe *.o *.obj *.pdb *.ilk
    echo "[handles] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[handles] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[handles] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 handles.cpp -Wl,/subsystem:console -o handles.exe
    TARGET="handles.exe"
else
    clang++ -std=c++17 -O2 handles.cpp -o handles
    TARGET="handles"
fi

echo "[handles] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[handles] Installed to ../../bin/$TARGET"
    fi
fi
