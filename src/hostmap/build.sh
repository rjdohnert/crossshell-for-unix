#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f hostmap hostmap.exe *.o *.obj *.pdb *.ilk
    echo "[hostmap] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[hostmap] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[hostmap] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 hostmap.cpp -lws2_32 -liphlpapi -ladvapi32 -Wl,/subsystem:console -o hostmap.exe
    TARGET="hostmap.exe"
else
    clang++ -std=c++17 -O2 hostmap.cpp -lws2_32 -liphlpapi -ladvapi32 -o hostmap
    TARGET="hostmap"
fi

echo "[hostmap] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[hostmap] Installed to ../../bin/$TARGET"
    fi
fi
