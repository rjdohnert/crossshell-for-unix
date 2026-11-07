#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f groupadd groupadd.exe *.o *.obj *.pdb *.ilk
    echo "[groupadd] Clean complete."
    exit 0
fi

if [ -n "$1" ] && [ "$1" != "build" ] && [ "$1" != "noinstall" ]; then
    echo "[groupadd] Error: unknown action '$1'." >&2
    exit 1
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[groupadd] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[groupadd] Compiling with clang++..."
if [[ "$OSTYPE" != "msys" && "$OSTYPE" != "cygwin" && "$OSTYPE" != "win32" ]]; then
    echo "[groupadd] Error: groupadd requires the Windows NetAPI." >&2
    exit 1
fi
clang++ -std=c++17 -O2 groupadd.cpp groupadd_cli.cpp groupadd_group.cpp groupadd_input.cpp groupadd_app.cpp -lnetapi32 -Wl,/subsystem:console -o groupadd.exe
TARGET="groupadd.exe"

echo "[groupadd] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/"
    echo "[groupadd] Installed to ../../bin/$TARGET"
fi
