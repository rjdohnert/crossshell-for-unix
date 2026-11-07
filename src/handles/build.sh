#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f handles handles.exe *.o *.obj *.pdb *.ilk
    echo "[handles] Clean complete."
    exit 0
fi

if [ -n "$1" ] && [ "$1" != "build" ] && [ "$1" != "noinstall" ]; then
    echo "[handles] Error: unknown action '$1'." >&2
    exit 1
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[handles] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[handles] Compiling with clang++..."
if [[ "$OSTYPE" != "msys" && "$OSTYPE" != "cygwin" && "$OSTYPE" != "win32" ]]; then
    echo "[handles] Error: handles requires the Windows process APIs." >&2
    exit 1
fi

clang++ -std=c++17 -O2 handles.cpp handles_cli.cpp handles_collect.cpp handles_format.cpp handles_app.cpp -Wl,/subsystem:console -o handles.exe
TARGET="handles.exe"

echo "[handles] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/"
    echo "[handles] Installed to ../../bin/$TARGET"
fi
