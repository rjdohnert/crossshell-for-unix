#!/usr/bin/env bash
set -e

TARGET="dmesg"
BIN_DIR="../../bin"
SRC="dmesg.cpp dmesg_app.cpp engine.cpp event_reader.cpp xml_helper.cpp options.cpp"

mkdir -p "$BIN_DIR"

if command -v clang++ >/dev/null 2>&1; then
    echo "Building $TARGET with clang++..."
    clang++ -std=c++17 -O2 $SRC -lwevtapi -o "$BIN_DIR/$TARGET"
    echo "Build successful: $BIN_DIR/$TARGET"
    exit 0
fi

if command -v g++ >/dev/null 2>&1; then
    echo "Building $TARGET with g++..."
    g++ -std=c++17 -O2 $SRC -lwevtapi -o "$BIN_DIR/$TARGET"
    echo "Build successful: $BIN_DIR/$TARGET"
    exit 0
fi

echo "Error: No suitable C++ compiler found (clang++ or g++)."
exit 1
