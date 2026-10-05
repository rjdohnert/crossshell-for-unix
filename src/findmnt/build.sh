#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f findmnt findmnt.exe *.o *.obj *.pdb *.ilk
    echo "[findmnt] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[findmnt] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[findmnt] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 findmnt.cpp -lmpr -Wl,/subsystem:console -o findmnt.exe
    TARGET="findmnt.exe"
else
    clang++ -std=c++17 -O2 findmnt.cpp -o findmnt
    TARGET="findmnt"
fi

echo "[findmnt] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[findmnt] Installed to ../../bin/$TARGET"
    fi
fi
