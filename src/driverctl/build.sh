#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f driverctl driverctl.exe *.o *.obj *.pdb *.ilk
    echo "[driverctl] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[driverctl] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[driverctl] Compiling with clang++..."
SOURCES=(
    driverctl.cpp
    driverctl_app.cpp
    driverctl_scanner.cpp
    driverctl_logger.cpp
    driverctl_decoder.cpp
    driverctl_formatters.cpp
    driverctl_utilities.cpp
)
case "$OSTYPE" in
    msys*|cygwin*|win32*)
        TARGET="driverctl.exe"
        clang++ -std=c++17 -O2 "${SOURCES[@]}" -Wl,/subsystem:console \
            -lsetupapi -lcfgmgr32 -ladvapi32 -lshell32 -lole32 -o "$TARGET"
        ;;
    *)
        echo "[driverctl] Error: driverctl requires a Windows or MSYS/Cygwin build environment." >&2
        exit 1
        ;;
esac

if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/driverctl.exe"
fi

echo "[driverctl] Build successful: $TARGET"
