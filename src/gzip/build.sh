#!/usr/bin/env bash
set -e
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SRC_DIR"
ACTION="${1:-build}"

if [ "$ACTION" = "clean" ]; then
    rm -f gzip gzip.exe *.o *.obj *.pdb *.ilk
    echo "[gzip] Clean complete."
    exit 0
fi

if ! command -v clang++ >/dev/null 2>&1; then
    echo "[gzip] Error: clang++ not found in PATH." >&2
    exit 1
fi

echo "[gzip] Compiling with clang++..."
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" || "$OSTYPE" == "win32" ]]; then
    clang++ -std=c++17 -O2 gzip.cpp gzip_cli.cpp gzip_codec.cpp gzip_file.cpp gzip_member.cpp gzip_report.cpp gzip_engine.cpp gzip_app.cpp -Wl,/subsystem:console -o gzip.exe
    TARGET="gzip.exe"
else
    echo "[gzip] Error: gzip currently requires the Windows PowerShell GZipStream runtime." >&2
    exit 1
fi

echo "[gzip] Build successful: $TARGET"

mkdir -p "../../bin"
cp -f "$TARGET" "../../bin/"
echo "[gzip] Installed to ../../bin/$TARGET"
