#!/usr/bin/env sh
set -e

TARGET="spawn.exe"
SOURCES="spawn.cpp vms_status.cpp spawn_options.cpp process_controller.cpp spawn_engine.cpp"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[spawn] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[spawn] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[spawn] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[spawn] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[spawn] Build failed." >&2
    exit 1
fi

echo "[spawn] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[spawn] Installed to $BIN_DIR/$TARGET"
    fi
fi
