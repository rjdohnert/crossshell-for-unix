#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f mkfs.exe *.o *.obj *.pdb *.ilk
    echo "[mkfs] Clean complete."
    exit 0
fi

if ! command -v clang >/dev/null 2>&1; then
    echo "[mkfs] Error: clang C compiler not found in PATH." >&2
    exit 1
fi

if [[ "$OSTYPE" != "msys" && "$OSTYPE" != "cygwin" && "$OSTYPE" != "win32" ]]; then
    echo "[mkfs] Error: mkfs uses Windows formatting APIs; build it from a Windows shell." >&2
    exit 1
fi

echo "[mkfs] Compiling C sources with clang..."
clang -std=c17 -O2 -Wall -Wextra -Wpedantic mkfs.c mkfs_cli.c mkfs_engine.c mkfs_error.c mkfs_fmifs.c -Wl,/subsystem:console -o mkfs.exe

echo "[mkfs] Build successful: mkfs.exe"
if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f mkfs.exe "../../bin/mkfs.exe"
    echo "[mkfs] Installed to bin/mkfs.exe"
fi
