#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f inetd inetd.exe *.o *.obj *.pdb *.ilk
    echo "[inetd] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[inetd] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[inetd] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 inetd.cpp -lws2_32 -lshell32 -ladvapi32 -Wl,/subsystem:console -o inetd.exe
    TARGET="inetd.exe"
else
    clang++ -std=c++17 -O2 inetd.cpp -lws2_32 -lshell32 -ladvapi32 -o inetd
    TARGET="inetd"
fi

echo "[inetd] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[inetd] Installed to ../../bin/$TARGET"
    fi
fi
