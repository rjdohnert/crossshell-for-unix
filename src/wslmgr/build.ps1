[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)

$ErrorActionPreference = 'Stop'
$ToolName = 'wslmgr'
$SourceFiles = @('apply_glyphs.cpp', 'apply_theme.cpp', 'make_palette.cpp', 'run_wsl_manager.cpp', 'wslmgr.cpp')
Push-Location $PSScriptRoot
try {
    if ($Clean) {
        Remove-Item *.obj, *.o, "$ToolName.exe", *.pdb, *.ilk -Force -ErrorAction SilentlyContinue
        return
    }
    if ($Compiler -eq 'auto') {
        if (Get-Command clang++ -ErrorAction SilentlyContinue) { $Compiler = 'clang' }
        elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $Compiler = 'cl' }
        else { throw 'A Windows C++17 compiler is required: clang++ or cl.exe.' }
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