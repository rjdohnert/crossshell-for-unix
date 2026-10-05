#!/usr/bin/env bash
set -e

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f kill kill.exe *.o *.obj *.pdb *.ilk
    echo "[kill] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[kill] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[kill] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 kill.cpp -ladvapi32 -luser32 -Wl,/subsystem:console -o kill.exe
    TARGET="kill.exe"
else
    clang++ -std=c++17 -O2 kill.cpp -ladvapi32 -luser32 -o kill
    TARGET="kill"
fi

echo "[kill] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[kill] Installed to ../../bin/$TARGET"
    fi
fi
