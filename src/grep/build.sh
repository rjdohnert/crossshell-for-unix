#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f grep grep.exe *.o *.obj *.pdb *.ilk
    echo "[grep] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[grep] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[grep] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 grep.cpp -lwbemuuid -lole32 -loleaut32 -ladvapi32 -Wl,/subsystem:console -o grep.exe
    TARGET="grep.exe"
else
    clang++ -std=c++17 -O2 grep.cpp -lwbemuuid -lole32 -loleaut32 -ladvapi32 -o grep
    TARGET="grep"
fi

echo "[grep] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[grep] Installed to ../../bin/$TARGET"
    fi
fi
