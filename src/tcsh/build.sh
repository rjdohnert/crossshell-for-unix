#!/usr/bin/env sh
set -e

TARGET="tcsh.exe"
SOURCES="tcsh.cpp tcsh_app.cpp options.cpp engine.cpp parser.cpp expansion.cpp builtins.cpp jobs.cpp terminal.cpp scripting.cpp selftest.cpp"
LIBS="-lshell32 -luser32 -ladvapi32"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[tcsh] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[tcsh] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
else
    echo "[tcsh] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[tcsh] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES $LIBS -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[tcsh] Build failed." >&2
    exit 1
fi

echo "[tcsh] Successfully built $TARGET"

if [ "$1" = "test" ] || [ "$1" = "--test" ]; then
    echo "[tcsh] Running internal self-tests..."
    ./"$TARGET" --self-test
fi

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[tcsh] Installed to $BIN_DIR/$TARGET"
    fi
fi
