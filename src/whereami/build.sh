#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TOOL_NAME="whereami"
SOURCE_FILES="address_formatter.cpp coordinates.cpp human_formatter.cpp json_formatter.cpp json_mini_parser.cpp network_geo_location_provider.cpp raw_formatter.cpp sexagesimal_formatter.cpp system_context_collector.cpp whereami_app.cpp whereami_help.cpp whereami.cpp windows_sensor_location_provider.cpp"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o "$TOOL_NAME.exe" *.pdb *.ilk; exit 0 ;;
        --no-install) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang++ >/dev/null 2>&1; then
    clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$TOOL_NAME.exe" $SOURCE_FILES -llocationapi -lwinhttp -liphlpapi -lws2_32 -lole32 -loleaut32 -luuid
elif command -v g++ >/dev/null 2>&1; then
    g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX   -o "$TOOL_NAME.exe" $SOURCE_FILES -llocationapi -lwinhttp -liphlpapi -lws2_32 -lole32 -loleaut32 -luuid
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:$TOOL_NAME.exe" $SOURCE_FILES locationapi.lib winhttp.lib iphlpapi.lib ws2_32.lib ole32.lib oleaut32.lib uuid.lib
else
    echo "No suitable Windows C++ compiler found (clang++, g++, or cl.exe required)." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f "$TOOL_NAME.exe" "../../bin/$TOOL_NAME.exe"
fi
