#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f htop htop.exe *.o *.obj *.pdb *.ilk
    echo "[htop] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[htop] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[htop] Compiling with clang++..."
TARGET="htop.exe"
clang++ -std=c++17 -O2 htop.cpp htop_app.cpp htop_telemetry.cpp htop_console.cpp htop_format.cpp -ladvapi32 -lpsapi -lntdll -Wl,/subsystem:console -o "$TARGET"

echo "[htop] Build successful: $TARGET"

if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/"
    echo "[htop] Installed to ../../bin/$TARGET"
fi
