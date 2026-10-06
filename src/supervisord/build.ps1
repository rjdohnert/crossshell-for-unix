[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl', 'gcc')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
$ToolName = 'supervisord'
$SourceFiles = @("admin_check.cpp", "async_pipe_pump.cpp", "config_diagnostics.cpp", "config_key_handlers.cpp", "config_parser.cpp", "config_validation.cpp", "config_value_parser.cpp", "environment_block.cpp", "event_ring_buffer.cpp", "executable_path.cpp", "health_check_scheduler_runtime.cpp", "ipc_action_parser.cpp", "ipc_client.cpp", "ipc_command_parser.cpp", "ipc_endpoint.cpp", "ipc_framing.cpp", "ipc_response.cpp", "logger.cpp", "managed_process.cpp", "path_encoding.cpp", "pipe_client_security.cpp", "process_state.cpp", "program_config_comparison.cpp", "rotating_file_sink.cpp", "scoped_handle.cpp", "service_events.cpp", "service_runtime.cpp", "supervisor.cpp", "supervisord_app.cpp", "supervisord_help.cpp", "supervisord.cpp")
Push-Location $PSScriptRoot
try {
    if ($Clean) {
        Remove-Item *.obj, *.o, "$ToolName.exe", *.pdb, *.ilk -Force -ErrorAction SilentlyContinue
        return
    }
    if ($Compiler -eq 'auto') {
        if (Get-Command clang++ -ErrorAction SilentlyContinue) { $Compiler = 'clang' }
        elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $Compiler = 'cl' }
        elseif (Get-Command g++ -ErrorAction SilentlyContinue) { $Compiler = 'gcc' }
        else { throw 'No suitable C++ compiler found (clang++, cl, or g++ required).' }
    }
    switch ($Compiler) {
        'clang' { & clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$ToolName.exe" @SourceFiles -ladvapi32 }
        'cl' { & cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:$ToolName.exe" @SourceFiles advapi32.lib }
        'gcc' { & g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -municode  -o "$ToolName.exe" @SourceFiles -ladvapi32 }
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $ToolName (exit $LASTEXITCODE)." }
    if (-not $NoInstall) {
        $BinDir = Join-Path $PSScriptRoot '../../bin'
        New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
        Copy-Item "$ToolName.exe" $BinDir -Force
    }
} finally { Pop-Location }
