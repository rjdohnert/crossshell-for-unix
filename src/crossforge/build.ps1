[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl', 'gcc')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
$ToolName = 'crossforge'
$SourceFiles = @(
    "crossforge.cpp",
    "theme_manager.cpp",
    "language_registry.cpp",
    "compiler_detector.cpp",
    "process_runner.cpp",
    "highlight_state.cpp",
    "document.cpp",
    "crossforge_engine.cpp"
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
        elseif (Get-Command g++ -ErrorAction SilentlyContinue) { $Compiler = 'gcc' }
        else { throw 'No suitable C++ compiler found (clang++, cl.exe, or g++ required).' }
    }
    switch ($Compiler) {
        'clang' { & clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$ToolName.exe" @SourceFiles -luser32 }
        'cl'    { & cl.exe /nologo /EHsc /std:c++17 /O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX "/Fe:$ToolName.exe" @SourceFiles user32.lib }
        'gcc'   { & g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$ToolName.exe" @SourceFiles -luser32 }
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $ToolName (exit $LASTEXITCODE)." }
    Write-Host "[$ToolName] Build successful: $ToolName.exe"
    if (-not $NoInstall) {
        $BinDir = Join-Path $PSScriptRoot '../../bin'
        New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
        Copy-Item "$ToolName.exe" $BinDir -Force
        Write-Host "[$ToolName] Installed to $BinDir\$ToolName.exe"
    }
} finally { Pop-Location }
