#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f getfacl getfacl.exe *.o *.obj *.pdb *.ilk
    echo "[getfacl] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[getfacl] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[getfacl] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 getfacl.cpp -ladvapi32 -Wl,/subsystem:console -o getfacl.exe
    TARGET="getfacl.exe"
else
    clang++ -std=c++17 -O2 getfacl.cpp -ladvapi32 -o getfacl
    TARGET="getfacl"
fi

echo "[getfacl] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    if [ -d "../../bin" ]; then
        cp -f "$TARGET" "../../bin/"
        echo "[getfacl] Installed to ../../bin/$TARGET"
    fi
fi
