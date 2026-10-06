[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl', 'gcc')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
$ToolName = 'wall'
$SourceFiles = @("banner_formatter.cpp", "session_broadcaster.cpp", "system_info_provider.cpp", "wall_app.cpp", "wall_config.cpp", "wall_engine.cpp", "wall.cpp")
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
        'clang' { & clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$ToolName.exe" @SourceFiles -lwtsapi32 -luser32 -ladvapi32 }
        'cl' { & cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:$ToolName.exe" @SourceFiles wtsapi32.lib user32.lib advapi32.lib }
        'gcc' { & g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX   -o "$ToolName.exe" @SourceFiles -lwtsapi32 -luser32 -ladvapi32 }
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $ToolName (exit $LASTEXITCODE)." }
    if (-not $NoInstall) {
        $BinDir = Join-Path $PSScriptRoot '../../bin'
        New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
        Copy-Item "$ToolName.exe" $BinDir -Force
    }
} finally { Pop-Location }
