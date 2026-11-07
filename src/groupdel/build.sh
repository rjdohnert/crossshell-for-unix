#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f groupdel.exe *.o *.obj *.pdb *.ilk
    echo "[groupdel] Clean complete."
    exit 0
fi

if [ -n "$1" ] && [ "$1" != "build" ] && [ "$1" != "noinstall" ]; then
    echo "[groupdel] Error: unknown action '$1'." >&2
    exit 1
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[groupdel] Error: clang++ not found in PATH." >&2
    exit 1
fi

if [[ "$OSTYPE" != "msys" && "$OSTYPE" != "cygwin" && "$OSTYPE" != "win32" ]]; then
    echo "[groupdel] Error: groupdel requires the Windows NetAPI." >&2
    exit 1
fi
echo "[groupdel] Compiling with clang++..."
clang++ -std=c++17 -O2 groupdel.cpp groupdel_cli.cpp groupdel_group.cpp groupdel_input.cpp groupdel_app.cpp -lnetapi32 -Wl,/subsystem:console -o groupdel.exe
TARGET="groupdel.exe"

echo "[groupdel] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/"
    echo "[groupdel] Installed to ../../bin/$TARGET"
fi
