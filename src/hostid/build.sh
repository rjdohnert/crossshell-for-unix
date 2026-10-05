#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f hostid hostid.exe *.o *.obj *.pdb *.ilk
    echo "[hostid] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[hostid] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[hostid] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 hostid.cpp -lws2_32 -ladvapi32 -Wl,/subsystem:console -o hostid.exe
    TARGET="hostid.exe"
else
    clang++ -std=c++17 -O2 hostid.cpp -lws2_32 -ladvapi32 -o hostid
    TARGET="hostid"
fi

echo "[hostid] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[hostid] Installed to ../../bin/$TARGET"
    fi
fi
