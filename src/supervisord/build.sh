#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
TOOL_NAME="supervisord"
SOURCE_FILES="admin_check.cpp async_pipe_pump.cpp config_diagnostics.cpp config_key_handlers.cpp config_parser.cpp config_validation.cpp config_value_parser.cpp environment_block.cpp event_ring_buffer.cpp executable_path.cpp health_check_scheduler_runtime.cpp ipc_action_parser.cpp ipc_client.cpp ipc_command_parser.cpp ipc_endpoint.cpp ipc_framing.cpp ipc_response.cpp logger.cpp managed_process.cpp path_encoding.cpp pipe_client_security.cpp process_state.cpp program_config_comparison.cpp rotating_file_sink.cpp scoped_handle.cpp service_events.cpp service_runtime.cpp supervisor.cpp supervisord_app.cpp supervisord_help.cpp supervisord.cpp"
NO_INSTALL=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean) rm -f *.obj *.o "$TOOL_NAME.exe" *.pdb *.ilk; exit 0 ;;
        --no-install) NO_INSTALL=1 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done
if command -v clang++ >/dev/null 2>&1; then
    clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$TOOL_NAME.exe" $SOURCE_FILES -ladvapi32
elif command -v g++ >/dev/null 2>&1; then
    g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -municode  -o "$TOOL_NAME.exe" $SOURCE_FILES -ladvapi32
elif command -v cl.exe >/dev/null 2>&1; then
    export MSYS_NO_PATHCONV=1
    cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:$TOOL_NAME.exe" $SOURCE_FILES advapi32.lib
else
    echo "No suitable Windows C++ compiler found (clang++, g++, or cl.exe required)." >&2
    exit 1
fi
if [ "$NO_INSTALL" -eq 0 ]; then
    mkdir -p ../../bin
    cp -f "$TOOL_NAME.exe" "../../bin/$TOOL_NAME.exe"
fi
