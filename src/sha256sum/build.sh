#!/usr/bin/env sh
set -e

TARGET="sha256sum.exe"
SOURCES="sha256sum.cpp sha256_digest.cpp sha256_options.cpp sha256_reporter.cpp checksum_verifier.cpp sha256_engine.cpp"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[sha256sum] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[sha256sum] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[sha256sum] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[sha256sum] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[sha256sum] Build failed." >&2
    exit 1
fi

echo "[sha256sum] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[sha256sum] Installed to $BIN_DIR/$TARGET"
    fi
fi
