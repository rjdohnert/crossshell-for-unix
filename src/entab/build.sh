#!/usr/bin/env bash
set -e

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "${1:-}" = "clean" ]; then
    rm -f entab entab.exe *.o *.obj *.pdb *.ilk
    echo "[entab] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[entab] Error: clang++ not found in PATH." >&2
    exit 1
fi

SOURCES=(
    entab.cpp
    entab_app.cpp
    entab_arg_parser.cpp
    entab_engine.cpp
    entab_help.cpp
    entab_reporter.cpp
)

if [[ "$OSTYPE" == msys* || "$OSTYPE" == cygwin* || "$OSTYPE" == win32* ]]; then
    TARGET="entab.exe"
    LINK_FLAGS=(-Wl,/subsystem:console)
else
    TARGET="entab"
    LINK_FLAGS=()
fi

echo "[entab] Compiling modules..."
clang++ -std=c++17 -O2 "${SOURCES[@]}" "${LINK_FLAGS[@]}" -o "$TARGET"
echo "[entab] Build successful: $TARGET"

if [ "${1:-}" != "noinstall" ]; then
    BIN_DIR="../../bin"
    mkdir -p "$BIN_DIR"
    cp -f "$TARGET" "$BIN_DIR/$TARGET"
    if [ ! -f "$BIN_DIR/$TARGET" ]; then
        echo "[entab] Error: failed to install $TARGET to bin/." >&2
        exit 1
    fi
fi
