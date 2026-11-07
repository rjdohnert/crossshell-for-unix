#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f fdisk.exe *.o *.obj *.pdb *.ilk
    echo "[fdisk] Clean complete."
    exit 0
fi

if ! command -v clang >/dev/null 2>&1; then
    echo "[fdisk] Error: clang C compiler not found in PATH." >&2
    exit 1
fi

if [[ "$OSTYPE" != "msys" && "$OSTYPE" != "cygwin" && "$OSTYPE" != "win32" ]]; then
    echo "[fdisk] Error: fdisk uses Windows disk APIs; build it from a Windows shell." >&2
    exit 1
fi

echo "[fdisk] Compiling C sources with clang..."
clang -std=c17 -O2 -Wall -Wextra -Wpedantic fdisk.c fdisk_app.c fdisk_device.c fdisk_editor.c fdisk_error.c fdisk_format.c fdisk_image.c fdisk_options.c fdisk_volume.c -Wl,/subsystem:console -lsetupapi -lole32 -lvirtdisk -o fdisk.exe

TARGET="fdisk.exe"
echo "[fdisk] Build successful: $TARGET"
if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/fdisk.exe"
fi
