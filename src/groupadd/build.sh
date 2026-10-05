#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f groupadd groupadd.exe *.o *.obj *.pdb *.ilk
    echo "[groupadd] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[groupadd] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[groupadd] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 groupadd.cpp -lnetapi32 -Wl,/subsystem:console -o groupadd.exe
    TARGET="groupadd.exe"
else
    clang++ -std=c++17 -O2 groupadd.cpp -lnetapi32 -o groupadd
    TARGET="groupadd"
fi

echo "[groupadd] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[groupadd] Installed to ../../bin/$TARGET"
    fi
fi
