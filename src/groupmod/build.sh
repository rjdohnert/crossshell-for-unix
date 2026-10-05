#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f groupmod groupmod.exe *.o *.obj *.pdb *.ilk
    echo "[groupmod] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[groupmod] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[groupmod] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 groupmod.cpp -lnetapi32 -Wl,/subsystem:console -o groupmod.exe
    TARGET="groupmod.exe"
else
    clang++ -std=c++17 -O2 groupmod.cpp -lnetapi32 -o groupmod
    TARGET="groupmod"
fi

echo "[groupmod] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[groupmod] Installed to ../../bin/$TARGET"
    fi
fi
