#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TOOL_NAME="typeset"
SOURCE_FILES="environment_store.cpp option_parser.cpp output_formatter.cpp registry_environment_store.cpp string_utils.cpp typeset_app.cpp typeset_pipe.cpp typeset.cpp"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o "$TOOL_NAME.exe" *.pdb *.ilk; exit 0 ;;
        --no-install) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang++ >/dev/null 2>&1; then
    clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$TOOL_NAME.exe" $SOURCE_FILES -ladvapi32 -luser32
elif command -v g++ >/dev/null 2>&1; then
    g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -municode  -o "$TOOL_NAME.exe" $SOURCE_FILES -ladvapi32 -luser32
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:$TOOL_NAME.exe" $SOURCE_FILES advapi32.lib user32.lib
else
    echo "No suitable Windows C++ compiler found (clang++, g++, or cl.exe required)." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f "$TOOL_NAME.exe" "../../bin/$TOOL_NAME.exe"
fi
