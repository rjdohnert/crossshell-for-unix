#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TOOL_NAME="unzip"
SOURCE_FILES="archive_extract.cpp archive_filter.cpp archive_list.cpp archive_test.cpp output_buffer.cpp output_session.cpp pipe_buffer.cpp powershell_quoting.cpp process_capture.cpp temporary_file.cpp unzip_app.cpp unzip_engine.cpp unzip_help.cpp unzip_option_parser.cpp unzip.cpp"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o "$TOOL_NAME.exe" *.pdb *.ilk; exit 0 ;;
        --no-install) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang++ >/dev/null 2>&1; then
    clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$TOOL_NAME.exe" $SOURCE_FILES 
elif command -v g++ >/dev/null 2>&1; then
    g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX   -o "$TOOL_NAME.exe" $SOURCE_FILES 
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:$TOOL_NAME.exe" $SOURCE_FILES 
else
    echo "No suitable Windows C++ compiler found (clang++, g++, or cl.exe required)." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f "$TOOL_NAME.exe" "../../bin/$TOOL_NAME.exe"
fi
