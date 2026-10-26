#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
SOURCE_FILES="rg.c search_state.c allocation.c options.c unicode.c literal_matcher.c regex_matcher.c pattern_set.c printer.c text_reader.c path_conversion.c file_search.c glob_matcher.c ignore_rules.c directory_walker.c help.c command_line.c file_types.c multiline_search.c parallel_search.c"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o rg.exe *.pdb *.ilk; exit 0 ;;
        --no-install) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang >/dev/null 2>&1; then
    clang -std=c17 -O2 -Wall -Wextra $SOURCE_FILES -o rg.exe
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /std:c17 /TC /O2 /W4 /Fe:rg.exe $SOURCE_FILES
else
    echo "A Windows C compiler is required: clang or cl.exe." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f rg.exe ../../bin/rg.exe
fi