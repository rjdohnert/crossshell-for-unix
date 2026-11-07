#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="diff3"
BIN_DIR="$SCRIPT_DIR/../../bin"

if [ -f "$SCRIPT_DIR/diff3.c" ]; then
    SRC="$SCRIPT_DIR/diff3.c"
elif [ -f "$SCRIPT_DIR/diff3_app.cpp" ]; then
    SRC="$SCRIPT_DIR/diff3.cpp $SCRIPT_DIR/diff3_app.cpp $SCRIPT_DIR/arg_parser.cpp $SCRIPT_DIR/formatter.cpp $SCRIPT_DIR/diff3_engine.cpp $SCRIPT_DIR/myers_diff.cpp $SCRIPT_DIR/line_reader.cpp"
else
    SRC="$SCRIPT_DIR/diff3.cpp"
fi

mkdir -p "$BIN_DIR"

if command -v clang >/dev/null 2>&1; then
    echo "Building $TARGET with clang..."
    clang -O2 $SRC -o "$BIN_DIR/$TARGET.exe"
    echo "Build successful: $BIN_DIR/$TARGET.exe"
    exit 0
fi

if command -v clang++ >/dev/null 2>&1; then
    echo "Building $TARGET with clang++..."
    clang++ -std=c++17 -O2 $SRC -o "$BIN_DIR/$TARGET.exe"
    echo "Build successful: $BIN_DIR/$TARGET.exe"
    exit 0
fi

if command -v gcc >/dev/null 2>&1; then
    echo "Building $TARGET with gcc..."
    gcc -O2 $SRC -o "$BIN_DIR/$TARGET.exe"
    echo "Build successful: $BIN_DIR/$TARGET.exe"
    exit 0
fi

if command -v g++ >/dev/null 2>&1; then
    echo "Building $TARGET with g++..."
    g++ -std=c++17 -O2 $SRC -o "$BIN_DIR/$TARGET.exe"
    echo "Build successful: $BIN_DIR/$TARGET.exe"
    exit 0
fi

echo "Error: No suitable compiler found (clang, clang++, gcc, or g++)."
exit 1
