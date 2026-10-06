#!/usr/bin/env sh
set -e

TARGET="strace.exe"
SOURCES="strace.cpp strace_app.cpp engine.cpp reporter.cpp options.cpp"
LIBS="-ldbghelp -Wl,/subsystem:console"
BIN_DIR="../../bin"

if [ "$1" = "clean" ] || [ "$1" = "--clean" ]; then
    echo "[strace] Cleaning build artifacts..."
    rm -f *.o *.obj *.pdb "$TARGET"
    echo "[strace] Clean complete."
    exit 0
fi

COMPILER=""
if command -v clang++ >/dev/null 2>&1; then
    COMPILER="clang++"
elif command -v g++ >/dev/null 2>&1; then
    COMPILER="g++"
    LIBS="-ldbghelp -municode"
else
    echo "[strace] Error: No suitable C++ compiler found (clang++, g++)." >&2
    exit 1
fi

echo "[strace] Building using $COMPILER..."
$COMPILER -std=c++17 -O2 -DNDEBUG $SOURCES $LIBS -o "$TARGET"

if [ ! -f "$TARGET" ]; then
    echo "[strace] Build failed." >&2
    exit 1
fi

echo "[strace] Successfully built $TARGET"

if [ "$1" != "--no-install" ]; then
    if [ -d "$BIN_DIR" ]; then
        cp -f "$TARGET" "$BIN_DIR/$TARGET"
        echo "[strace] Installed to $BIN_DIR/$TARGET"
    fi
fi
