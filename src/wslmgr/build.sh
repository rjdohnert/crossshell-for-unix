#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TOOL_NAME="wslmgr"
SOURCE_FILES="apply_glyphs.cpp apply_theme.cpp make_palette.cpp run_wsl_manager.cpp wslmgr.cpp"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o "$TOOL_NAME.exe" *.pdb *.ilk; exit 0 ;;
        --no-install|-NoInstall|noinstall) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang++ >/dev/null 2>&1; then
    clang++ -std=c++17 -O2 -o "$TOOL_NAME.exe" $SOURCE_FILES
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /std:c++17 /O2 /EHsc /W4 "/Fe:$TOOL_NAME.exe" $SOURCE_FILES
else
    echo "A Windows C++17 compiler is required (clang++ or cl.exe)." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f "$TOOL_NAME.exe" "../../bin/$TOOL_NAME.exe"
fi