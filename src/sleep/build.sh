#!/usr/bin/env sh
set -e

TARGET="sleep.exe"
SOURCES="sleep.cpp duration_parser.cpp telemetry_reporter.cpp sleep_options.cpp sleep_engine.cpp"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[sleep] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[sleep] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[sleep] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[sleep] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[sleep] Build failed." >&2
    exit 1
fi

echo "[sleep] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[sleep] Installed to $BIN_DIR/$TARGET"
    fi
fi
