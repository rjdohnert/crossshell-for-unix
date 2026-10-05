#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f fsctl fsctl.exe *.o *.obj *.pdb *.ilk
    echo "[fsctl] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[fsctl] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[fsctl] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 fsctl.cpp -lmpr -lwininet -lshell32 -ladvapi32 -Wl,/subsystem:console -o fsctl.exe
    TARGET="fsctl.exe"
else
    clang++ -std=c++17 -O2 fsctl.cpp -lmpr -lwininet -lshell32 -ladvapi32 -o fsctl
    TARGET="fsctl"
fi

echo "[fsctl] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[fsctl] Installed to ../../bin/$TARGET"
    fi
fi
