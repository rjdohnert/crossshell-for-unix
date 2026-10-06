[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl', 'gcc')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
$ToolName = 'tmux'
$SourceFiles = @("border_renderer.cpp", "cell.cpp", "clipboard.cpp", "code_point_width.cpp", "color.cpp", "console_session.cpp", "frame_renderer.cpp", "key_binding_parser.cpp", "key_combo.cpp", "key_event_encoder.cpp", "layout_node.cpp", "mouse_router.cpp", "pane_focus.cpp", "pane.cpp", "rect.cpp", "shell_command.cpp", "string_encoding.cpp", "string_utils.cpp", "theme_catalog.cpp", "theme_lookup.cpp", "tmux_app.cpp", "tmux_config.cpp", "tmux_help.cpp", "tmux.cpp", "virtual_screen.cpp", "window_manager.cpp", "window.cpp", "wmux_engine.cpp")
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
        'clang' { & clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$ToolName.exe" @SourceFiles -luser32 }
        'cl' { & cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:$ToolName.exe" @SourceFiles user32.lib }
        'gcc' { & g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX   -o "$ToolName.exe" @SourceFiles -luser32 }
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $ToolName (exit $LASTEXITCODE)." }
    if (-not $NoInstall) {
        $BinDir = Join-Path $PSScriptRoot '../../bin'
        New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
        Copy-Item "$ToolName.exe" $BinDir -Force
    }
} finally { Pop-Location }
