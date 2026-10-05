#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f groupdel groupdel.exe *.o *.obj *.pdb *.ilk
    echo "[groupdel] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[groupdel] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[groupdel] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 groupdel.cpp -lnetapi32 -Wl,/subsystem:console -o groupdel.exe
    TARGET="groupdel.exe"
else
    clang++ -std=c++17 -O2 groupdel.cpp -lnetapi32 -o groupdel
    TARGET="groupdel"
fi

echo "[groupdel] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[groupdel] Installed to ../../bin/$TARGET"
    fi
fi
