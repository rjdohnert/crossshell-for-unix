#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SOURCE_FILES="pv.c pv_time.c pv_format.c pv_config.c pv_display.c pv_engine.c"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o pv.exe *.pdb *.ilk; exit 0 ;;
        --no-install) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang >/dev/null 2>&1; then
    clang -std=c17 -O2 -Wall -Wextra $SOURCE_FILES -o pv.exe
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /std:c17 /TC /O2 /W4 /Fe:pv.exe $SOURCE_FILES
else
    echo "A Windows C compiler is required: clang or cl.exe." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f pv.exe ../../bin/pv.exe
    rm -f *.obj *.o pv.exe *.pdb *.ilk
fi