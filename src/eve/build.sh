#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

ACTION="${1:-build}"

if [ "$ACTION" = "clean" ]; then
    rm -f eve eve.exe *.o *.obj *.pdb *.ilk
    echo "[eve] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[eve] Error: clang++ not found in PATH." >&2
    exit 1
fi

case "$OSTYPE" in
    msys*|cygwin*|win32*)
        echo "[eve] Compiling modular sources with clang++..."
        clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic \
            eve.cpp eve_app.cpp eve_console.cpp eve_editor.cpp eve_file_io.cpp eve_pipe.cpp \
            -Wl,/subsystem:console -o eve.exe
        TARGET="eve.exe"
        ;;
    *)
        echo "[eve] Error: EVE requires the Windows console APIs; use a Windows-compatible shell." >&2
        exit 1
        ;;
esac

echo "[eve] Build successful: $TARGET"
if [ "$ACTION" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/eve.exe"
    echo "[eve] Installed: ../../bin/eve.exe"
fi
