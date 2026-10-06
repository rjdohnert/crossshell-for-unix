#!/usr/bin/env sh
set -e

TARGET="seq.exe"
SOURCES="seq.cpp number_parser.cpp seq_options.cpp number_formatter.cpp sequence_generator.cpp"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[seq] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[seq] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[seq] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[seq] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[seq] Build failed." >&2
    exit 1
fi

echo "[seq] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[seq] Installed to $BIN_DIR/$TARGET"
    fi
fi
