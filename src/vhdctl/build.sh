#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TOOL_NAME="vhdctl"
SOURCE_FILES="scoped_process_token.cpp scoped_sid.cpp scoped_virtual_disk_handle.cpp scoped_wide_pipe_redirect.cpp vhd_operations.cpp vhd_reporter.cpp vhd_storage_inspector.cpp vhdctl_app.cpp vhdctl_options.cpp vhdctl.cpp virtual_disk_guids.cpp wide_pipe_buffer.cpp"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o "$TOOL_NAME.exe" *.pdb *.ilk; exit 0 ;;
        --no-install) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang++ >/dev/null 2>&1; then
    clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$TOOL_NAME.exe" $SOURCE_FILES -lvirtdisk -ladvapi32
elif command -v g++ >/dev/null 2>&1; then
    g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -municode  -o "$TOOL_NAME.exe" $SOURCE_FILES -lvirtdisk -ladvapi32
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:$TOOL_NAME.exe" $SOURCE_FILES virtdisk.lib advapi32.lib
else
    echo "No suitable Windows C++ compiler found (clang++, g++, or cl.exe required)." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f "$TOOL_NAME.exe" "../../bin/$TOOL_NAME.exe"
fi
