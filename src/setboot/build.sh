#!/usr/bin/env sh
set -e

TARGET="setboot.exe"
SOURCES="setboot.cpp setboot_options.cpp nvram_controller.cpp boot_reporter.cpp"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[setboot] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[setboot] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[setboot] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[setboot] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES -ladvapi32 -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[setboot] Build failed." >&2
    exit 1
fi

echo "[setboot] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[setboot] Installed to $BIN_DIR/$TARGET"
    fi
fi
