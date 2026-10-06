#!/usr/bin/env sh
set -e

TARGET="sequence.exe"
SOURCES="sequence.cpp sequence_options.cpp field_extractor.cpp key_comparator.cpp sort_pipeline.cpp"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[sequence] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[sequence] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[sequence] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[sequence] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[sequence] Build failed." >&2
    exit 1
fi

echo "[sequence] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[sequence] Installed to $BIN_DIR/$TARGET"
    fi
fi
