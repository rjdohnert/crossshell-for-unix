#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f groupmod.exe *.o *.obj *.pdb *.ilk
    echo "[groupmod] Clean complete."
    exit 0
fi

if [ -n "$1" ] && [ "$1" != "build" ] && [ "$1" != "noinstall" ]; then
    echo "[groupmod] Error: unknown action '$1'." >&2
    exit 1
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[groupmod] Error: clang++ not found in PATH." >&2
    exit 1
fi

if [[ "$OSTYPE" != "msys" && "$OSTYPE" != "cygwin" && "$OSTYPE" != "win32" ]]; then
    echo "[groupmod] Error: groupmod requires the Windows NetAPI." >&2
    exit 1
fi
echo "[groupmod] Compiling with clang++..."
clang++ -std=c++17 -O2 groupmod.cpp groupmod_cli.cpp groupmod_group.cpp groupmod_input.cpp groupmod_app.cpp -lnetapi32 -Wl,/subsystem:console -o groupmod.exe
TARGET="groupmod.exe"

echo "[groupmod] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/"
    echo "[groupmod] Installed to ../../bin/$TARGET"
fi
