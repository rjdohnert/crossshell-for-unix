#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f env env.exe *.o *.obj *.pdb *.ilk
    echo "[env] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[env] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[env] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 env.cpp -Wl,/subsystem:console -o env.exe
    TARGET="env.exe"
else
    clang++ -std=c++17 -O2 env.cpp -o env
    TARGET="env"
fi

echo "[env] Build successful: $TARGET"
if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
    fi
fi
