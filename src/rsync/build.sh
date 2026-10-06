#!/usr/bin/env sh
set -e

TARGET="rsync.exe"
SOURCES="rsync.cpp rsync_options.cpp delta_engine.cpp pipe_stream.cpp sync_engine.cpp"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[rsync] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[rsync] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[rsync] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[rsync] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[rsync] Build failed." >&2
    exit 1
fi

echo "[rsync] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[rsync] Installed to $BIN_DIR/$TARGET"
    fi
fi
