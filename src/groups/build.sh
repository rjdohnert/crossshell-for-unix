#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f groups groups.exe *.o *.obj *.pdb *.ilk
    echo "[groups] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[groups] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[groups] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 groups.cpp -lnetapi32 -ladvapi32 -Wl,/subsystem:console -o groups.exe
    TARGET="groups.exe"
else
    clang++ -std=c++17 -O2 groups.cpp -lnetapi32 -ladvapi32 -o groups
    TARGET="groups"
fi

echo "[groups] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[groups] Installed to ../../bin/$TARGET"
    fi
fi
