#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TOOL_NAME="winobj"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o "$TOOL_NAME.exe" *.pdb *.ilk; exit 0 ;;
        --no-install|-NoInstall|noinstall) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang >/dev/null 2>&1; then
    clang -std=c17 -O2 -Wall -Wextra -o "$TOOL_NAME.exe" winobj.c -ladvapi32
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /O2 /W4 /TC winobj.c "/Fe:$TOOL_NAME.exe"
elif command -v gcc >/dev/null 2>&1; then
    gcc -std=c17 -O2 -Wall -Wextra -municode -o "$TOOL_NAME.exe" winobj.c -ladvapi32
else
    echo "No suitable Windows C compiler found (clang, cl.exe, or gcc required)." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f "$TOOL_NAME.exe" "../../bin/$TOOL_NAME.exe"
fi