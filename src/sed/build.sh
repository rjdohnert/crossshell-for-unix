#!/usr/bin/env sh
set -e

TARGET="sed.exe"
SOURCES="sed.cpp sed_app.cpp engine.cpp parser.cpp options.cpp"
LIBS="-lole32 -loleaut32 -lwbemuuid -ladvapi32"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[sed] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[sed] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[sed] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[sed] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES $LIBS -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[sed] Build failed." >&2
    exit 1
fi

echo "[sed] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[sed] Installed to $BIN_DIR/$TARGET"
    fi
fi
