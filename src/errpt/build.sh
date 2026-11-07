#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f errpt errpt.exe *.o *.obj *.pdb *.ilk
    echo "[errpt] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[errpt] Error: clang++ not found in PATH." >&2
    exit 1
fi

if [[ "$OSTYPE" != msys* && "$OSTYPE" != cygwin* && "$OSTYPE" != win32* ]]; then
    echo "[errpt] Error: errpt uses the Windows Event Log API; build it with build.ps1 or build.bat on Windows." >&2
    exit 1
fi

SOURCES=(
    errpt.cpp
    errpt_application.cpp
    errpt_event_reader.cpp
    errpt_output.cpp
    errpt_parser.cpp
)

TARGET="errpt.exe"
echo "[errpt] Compiling Windows modules with clang++..."
clang++ -std=c++17 -O2 "${SOURCES[@]}" -Wl,/subsystem:console -o "$TARGET"

echo "[errpt] Build successful: $TARGET"
if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/$TARGET"
    if [ ! -f "../../bin/$TARGET" ]; then
        echo "[errpt] Error: failed to install $TARGET to bin/." >&2
        exit 1
    fi
fi
