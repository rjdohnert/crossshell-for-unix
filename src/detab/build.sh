#!/usr/bin/env bash
set -e

TARGET="detab"
BIN_DIR="../../bin"
SRC="detab.cpp detab_app.cpp engine.cpp reporter.cpp options.cpp"

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
