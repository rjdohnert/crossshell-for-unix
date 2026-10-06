#!/usr/bin/env sh
set -e

TARGET="sar.exe"
SOURCES="sar.cpp sar_options.cpp system_sampler.cpp report_formatter.cpp"
LIBS="-lpdh -liphlpapi -lpsapi"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[sar] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[sar] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[sar] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[sar] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES $LIBS -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[sar] Build failed." >&2
    exit 1
fi

echo "[sar] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[sar] Installed to $BIN_DIR/$TARGET"
    fi
fi
