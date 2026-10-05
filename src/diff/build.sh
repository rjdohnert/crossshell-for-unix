#!/usr/bin/env bash
set -e

TARGET="diff"
BIN_DIR="../../bin"
SRC="diff.cpp diff_app.cpp arg_parser.cpp formatter.cpp myers_diff.cpp line_reader.cpp normalizer.cpp"

mkdir -p "$BIN_DIR"

if command -v clang++ >/dev/null 2>&1; then
    echo "Building $TARGET with clang++..."
    clang++ -std=c++17 -O2 $SRC -o "$BIN_DIR/$TARGET"
    echo "Build successful: $BIN_DIR/$TARGET"
    exit 0
fi

if command -v g++ >/dev/null 2>&1; then
    echo "Building $TARGET with g++..."
    g++ -std=c++17 -O2 $SRC -o "$BIN_DIR/$TARGET"
    echo "Build successful: $BIN_DIR/$TARGET"
    exit 0
fi

echo "Error: No suitable C++ compiler found (clang++ or g++)."
exit 1
