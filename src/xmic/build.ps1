[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)

$ErrorActionPreference = 'Stop'
$ToolName = 'xmic'
$SourceFiles = @(
    'capture_process_output.cpp', 'command_line_helpers.cpp',
    'execute_ram_status_summary.cpp', 'execute_virtual_switch_query.cpp',
    'format_wmi_output.cpp', 'format_wmi_values.cpp', 'parse_configuration.cpp',
    'print_help.cpp', 'run_interactive_shell.cpp', 'run_wsl_detection_self_tests.cpp',
    'ssh_transport.cpp', 'winrm_transport.cpp', 'wmi_connection.cpp', 'wmi_queries.cpp',
    'wsl_detection.cpp', 'xmic.cpp', 'xmic_state.cpp'
)
Push-Location $PSScriptRoot
try {
    if ($Clean) {
        Remove-Item *.obj, *.o, "$ToolName.exe", *.pdb, *.ilk -Force -ErrorAction SilentlyContinue
        return
    }
    if ($Compiler -eq 'auto') {
        if (Get-Command clang++ -ErrorAction SilentlyContinue) { $Compiler = 'clang' }
        elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $Compiler = 'cl' }
        else { throw 'A Windows C++17 compiler with WMI/MI SDK libraries is required.' }
    }
    if ($Compiler -eq 'clang') {
        & clang++ -std=c++17 -O2 -o "$ToolName.exe" @SourceFiles
    } else {
        & cl.exe /nologo /std:c++17 /O2 /EHsc /W4 "/Fe:$ToolName.exe" @SourceFiles
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $ToolName (exit $LASTEXITCODE)." }
    if (-not $NoInstall) {
        $BinDir = Join-Path $PSScriptRoot '../../bin'
        New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
        Copy-Item "$ToolName.exe" $BinDir -Force
    }
} finally {
    Pop-Location
}