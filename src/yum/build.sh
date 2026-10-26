#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SOURCE_FILES="yum.c command_line.c command_line_escaping.c winget_dispatch.c windows_update_query.c privilege_check.c windows_update_install.c package_install.c package_remove.c package_search.c package_info.c package_list.c package_upgrade.c repository_list.c repository_add.c repository_remove.c help.c"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o yum.exe *.pdb *.ilk; exit 0 ;;
        --no-install) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang >/dev/null 2>&1; then
    clang -std=c17 -O2 -Wall -Wextra $SOURCE_FILES -o yum.exe
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /std:c17 /TC /O2 /W4 /Fe:yum.exe $SOURCE_FILES
else
    echo "A Windows C compiler is required: clang or cl.exe." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f yum.exe ../../bin/yum.exe
fi