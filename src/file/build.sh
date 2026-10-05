#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f file file.exe *.o *.obj *.pdb *.ilk
    echo "[file] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[file] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[file] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 file.cpp -Wl,/subsystem:console -o file.exe
    TARGET="file.exe"
else
    clang++ -std=c++17 -O2 file.cpp -o file
    TARGET="file"
fi

echo "[file] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[file] Installed to ../../bin/$TARGET"
    fi
fi
