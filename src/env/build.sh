#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"

if [ "$1" = "clean" ]; then
    rm -f env env.exe *.o *.obj *.pdb *.ilk
    echo "[env] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[env] Error: clang++ not found in PATH." >&2
    exit 1
fi

if [[ "$OSTYPE" != msys* && "$OSTYPE" != cygwin* && "$OSTYPE" != win32* ]]; then
    echo "[env] Error: env uses the Windows process API; build it with build.ps1 or build.bat on Windows." >&2
    exit 1
fi

SOURCES=(
    env.cpp
    env_arguments.cpp
    env_application.cpp
    env_environment.cpp
    env_output.cpp
    env_process.cpp
)

TARGET="env.exe"
echo "[env] Compiling Windows modules with clang++..."
clang++ -std=c++17 -O2 "${SOURCES[@]}" -Wl,/subsystem:console -o "$TARGET"

echo "[env] Build successful: $TARGET"
if [ "$1" != "noinstall" ]; then
    mkdir -p "../../bin"
    cp -f "$TARGET" "../../bin/$TARGET"
    if [ ! -f "../../bin/$TARGET" ]; then
        echo "[env] Error: failed to install $TARGET to bin/." >&2
        exit 1
    fi
fi
