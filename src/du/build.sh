#!/usr/bin/env bash
set -e

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "${1:-}" = "clean" ]; then
    rm -f du du.exe *.o *.obj *.pdb *.ilk
    echo "[du] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[du] Error: clang++ not found in PATH." >&2
    exit 1
fi

SOURCES=(
    du.cpp
    du_app.cpp
    du_arg_parser.cpp
    du_help_system.cpp
    du_input_stream_reader.cpp
    du_path_filter.cpp
    du_reporter.cpp
    du_scanner.cpp
    du_size_formatter.cpp
)

if [[ "$OSTYPE" == msys* || "$OSTYPE" == cygwin* || "$OSTYPE" == win32* ]]; then
    TARGET="du.exe"
    LINK_FLAGS=(-Wl,/subsystem:console)
else
    TARGET="du"
    LINK_FLAGS=()
fi

echo "[du] Compiling modules..."
clang++ -std=c++17 -O2 "${SOURCES[@]}" "${LINK_FLAGS[@]}" -o "$TARGET"
echo "[du] Build successful: $TARGET"

if [ "${1:-}" != "noinstall" ]; then
    BIN_DIR="../../bin"
    mkdir -p "$BIN_DIR"
    cp -f "$TARGET" "$BIN_DIR/$TARGET"
    if [ ! -f "$BIN_DIR/$TARGET" ]; then
        echo "[du] Error: failed to install $TARGET to bin/." >&2
        exit 1
    fi
fi
