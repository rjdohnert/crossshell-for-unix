#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TOOL_NAME="xmic"
SOURCE_FILES="capture_process_output.cpp command_line_helpers.cpp execute_ram_status_summary.cpp execute_virtual_switch_query.cpp format_wmi_output.cpp format_wmi_values.cpp parse_configuration.cpp print_help.cpp run_interactive_shell.cpp run_wsl_detection_self_tests.cpp ssh_transport.cpp winrm_transport.cpp wmi_connection.cpp wmi_queries.cpp wsl_detection.cpp xmic.cpp xmic_state.cpp"
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
    echo "A Windows C++17 compiler with WMI/MI SDK libraries is required." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f "$TOOL_NAME.exe" "../../bin/$TOOL_NAME.exe"
fi