#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"
ACTION="${1:-build}"

if [ "$ACTION" = "clean" ]; then
    rm -f gsar gsar.exe *.o *.obj *.pdb *.ilk
    echo "[gsar] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[gsar] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[gsar] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 gsar.cpp gsar_pattern.cpp gsar_cli.cpp gsar_engine.cpp gsar_file.cpp gsar_app.cpp -Wl,/subsystem:console -o gsar.exe
    TARGET="gsar.exe"
else
    clang++ -std=c++17 -O2 gsar.cpp gsar_pattern.cpp gsar_cli.cpp gsar_engine.cpp gsar_file.cpp gsar_app.cpp -o gsar
    TARGET="gsar"
fi

echo "[gsar] Build successful: $TARGET"

mkdir -p "../../bin"
cp -f "$TARGET" "../../bin/"
echo "[gsar] Installed to ../../bin/$TARGET"
