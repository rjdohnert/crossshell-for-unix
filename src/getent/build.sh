#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f getent getent.exe *.o *.obj *.pdb *.ilk
    echo "[getent] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[getent] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[getent] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 getent.cpp -lnetapi32 -lws2_32 -liphlpapi -ladvapi32 -Wl,/subsystem:console -o getent.exe
    TARGET="getent.exe"
else
    clang++ -std=c++17 -O2 getent.cpp -lnetapi32 -lws2_32 -liphlpapi -ladvapi32 -o getent
    TARGET="getent"
fi

echo "[getent] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[getent] Installed to ../../bin/$TARGET"
    fi
fi
