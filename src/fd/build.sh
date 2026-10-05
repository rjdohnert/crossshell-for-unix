#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f fd fd.exe *.o *.obj *.pdb *.ilk
    echo "[fd] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[fd] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[fd] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 fd.cpp -Wl,/subsystem:console -o fd.exe
    TARGET="fd.exe"
else
    clang++ -std=c++17 -O2 fd.cpp -o fd
    TARGET="fd"
fi

echo "[fd] Build successful: $TARGET"
if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
    fi
fi
