#!/usr/bin/env sh
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN_DIR="$ROOT_DIR/bin"

mkdir -p "$BIN_DIR"

case "$(uname -s 2>/dev/null || echo Windows)" in
    CYGWIN*|MINGW*|MSYS*|Windows*)
        TARGET="$BIN_DIR/zstd.exe"
        ;;
    *)
        TARGET="$BIN_DIR/zstd"
        ;;
esac

echo "========================================================"
echo "Building zstd - CrossShell RFC 8878 Enterprise"
echo "========================================================"

SOURCES="$SCRIPT_DIR/zstd.c $SCRIPT_DIR/xxh64.c $SCRIPT_DIR/codec.c $SCRIPT_DIR/pool.c $SCRIPT_DIR/engine.c $SCRIPT_DIR/zstddeclib.c"

if command -v clang >/dev/null 2>&1; then
    echo "Compiling with Clang -O3..."
    clang -O3 -Wall -Wextra -Wno-unused-function -Wno-unused-const-variable $SOURCES -o "$TARGET"
elif command -v gcc >/dev/null 2>&1; then
    echo "Compiling with GCC -O3..."
    gcc -O3 -Wall -Wextra $SOURCES -o "$TARGET"
elif command -v cc >/dev/null 2>&1; then
    echo "Compiling with CC -O3..."
    cc -O3 -Wall -Wextra $SOURCES -o "$TARGET"
else
    echo "ERROR: No C compiler found (clang, gcc, cc)." >&2
    exit 1
fi

echo "[SUCCESS] Built: $TARGET"
