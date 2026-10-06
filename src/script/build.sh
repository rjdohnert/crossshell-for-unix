#!/usr/bin/env sh
set -e

TARGET="script.exe"
SOURCES="script.cpp script_options.cpp transcript_writer.cpp session_recorder.cpp"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[script] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[script] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[script] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[script] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[script] Build failed." >&2
    exit 1
fi

echo "[script] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[script] Installed to $BIN_DIR/$TARGET"
    fi
fi
