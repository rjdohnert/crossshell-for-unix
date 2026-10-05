#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f ipadm ipadm.exe *.o *.obj *.pdb *.ilk
    echo "[ipadm] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[ipadm] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[ipadm] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 ipadm.cpp -liphlpapi -lws2_32 -Wl,/subsystem:console -o ipadm.exe
    TARGET="ipadm.exe"
else
    clang++ -std=c++17 -O2 ipadm.cpp -liphlpapi -lws2_32 -o ipadm
    TARGET="ipadm"
fi

echo "[ipadm] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[ipadm] Installed to ../../bin/$TARGET"
    fi
fi
