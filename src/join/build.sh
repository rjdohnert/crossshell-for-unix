#!/usr/bin/env bash
set -e

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f join join.exe *.o *.obj *.pdb *.ilk
    echo "[join] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[join] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[join] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 join.cpp -Wl,/subsystem:console -o join.exe
    TARGET="join.exe"
else
    clang++ -std=c++17 -O2 join.cpp -o join
    TARGET="join"
fi

echo "[join] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[join] Installed to ../../bin/$TARGET"
    fi
fi
