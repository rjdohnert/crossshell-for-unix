#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="diff"
BIN_DIR="$SCRIPT_DIR/../../bin"
SRC="$SCRIPT_DIR/diff.c"

mkdir -p "$BIN_DIR"

if command -v clang >/dev/null 2>&1; then
    echo "Building $TARGET with clang..."
    clang -O2 "$SRC" -o "$BIN_DIR/$TARGET.exe"
    echo "Build successful: $BIN_DIR/$TARGET.exe"
    exit 0
fi

if command -v gcc >/dev/null 2>&1; then
    echo "Building $TARGET with gcc..."
    gcc -O2 "$SRC" -o "$BIN_DIR/$TARGET.exe"
    echo "Build successful: $BIN_DIR/$TARGET.exe"
    exit 0
fi

echo "Error: No suitable C compiler found (clang or gcc)."
exit 1
