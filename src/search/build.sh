#!/usr/bin/env sh
set -e

TARGET="search.exe"
SOURCES="search.cpp terminal_format.cpp known_folders.cpp pattern_matcher.cpp search_options.cpp search_reporter.cpp search_engine.cpp"
LIBS="-lshell32 -lole32"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[search] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[search] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[search] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[search] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES $LIBS -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[search] Build failed." >&2
    exit 1
fi

echo "[search] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[search] Installed to $BIN_DIR/$TARGET"
    fi
fi
