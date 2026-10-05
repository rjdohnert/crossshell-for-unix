#!/usr/bin/env bash
set -e

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f kgdb kgdb.exe *.o *.obj *.pdb *.ilk
    echo "[kgdb] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[kgdb] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[kgdb] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 kgdb.cpp -lws2_32 -lpsapi -Wl,/subsystem:console -o kgdb.exe
    TARGET="kgdb.exe"
else
    clang++ -std=c++17 -O2 kgdb.cpp -lws2_32 -lpsapi -o kgdb
    TARGET="kgdb"
fi

echo "[kgdb] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[kgdb] Installed to ../../bin/$TARGET"
    fi
fi
