#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f dskctl dskctl.exe *.o *.obj *.pdb *.ilk
    echo "[dskctl] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[dskctl] Error: clang++ not found in PATH." >&2
    exit 1
fi

case "$OSTYPE" in
    msys*|cygwin*|win32*) ;;
    *)
        echo "[dskctl] Error: dskctl requires a Windows or MSYS/Cygwin build environment." >&2
        exit 1
        ;;
esac

echo "[dskctl] Compiling with clang++..."
clang++ -std=c++17 -O2 dskctl.cpp dskctl_app.cpp dskctl_engine.cpp dskctl_helpers.cpp dskctl_logger.cpp dskctl_options.cpp dskctl_reporter.cpp dskctl_security.cpp dskctl_targets.cpp -Wl,/subsystem:console -lshell32 -ladvapi32 -lole32 -o dskctl.exe
TARGET="dskctl.exe"

if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/dskctl.exe"
fi

echo "[dskctl] Build successful: $TARGET"
