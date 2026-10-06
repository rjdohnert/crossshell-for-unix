#!/usr/bin/env sh
set -e

TARGET="runcon.exe"
SOURCES="runcon.cpp runcon_options.cpp token_privilege.cpp process_launcher.cpp"
LIBS="-ladvapi32"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[runcon] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[runcon] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[runcon] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[runcon] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES $LIBS -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[runcon] Build failed." >&2
    exit 1
fi

echo "[runcon] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[runcon] Installed to $BIN_DIR/$TARGET"
    fi
fi
