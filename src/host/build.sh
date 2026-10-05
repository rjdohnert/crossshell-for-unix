#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f host host.exe *.o *.obj *.pdb *.ilk
    echo "[host] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[host] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[host] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 host.cpp -ldnsapi -lws2_32 -Wl,/subsystem:console -o host.exe
    TARGET="host.exe"
else
    clang++ -std=c++17 -O2 host.cpp -ldnsapi -lws2_32 -o host
    TARGET="host"
fi

echo "[host] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[host] Installed to ../../bin/$TARGET"
    fi
fi
