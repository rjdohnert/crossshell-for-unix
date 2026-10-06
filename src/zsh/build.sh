#!/usr/bin/env sh
set -e

TARGET="zsh.exe"
SOURCES="engine.cpp parser.cpp expansion.cpp jobs.cpp terminal.cpp builtins.cpp scripting.cpp selftest.cpp zsh.cpp"
LIBS="-lshell32 -luser32 -ladvapi32 -Wl,/STACK:8388608"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[zsh] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[zsh] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[zsh] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[zsh] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES $LIBS -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[zsh] Build failed." >&2
    exit 1
fi

echo "[zsh] Successfully built $TARGET"

if [ "$1" = "test" ] || [ "$1" = "--test" ]; then
    echo "[zsh] Running internal self-tests..."
    ./"$TARGET" --self-test
fi

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[zsh] Installed to $BIN_DIR/$TARGET"
    fi
fi
