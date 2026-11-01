[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl', 'gcc')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
$ToolName = 'conexec'
$SourceFiles = @(
    "conexec.cpp",
    "conexec_main.cpp",
    "handle_closer.cpp",
    "security_engine.cpp",
    "execution_dispatcher.cpp"
)
$Libs = @("advapi32", "user32", "shell32", "userenv")
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
        'clang' { & clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$ToolName.exe" @SourceFiles -ladvapi32 -luser32 -lshell32 -luserenv }
        'cl'    { & cl.exe /nologo /EHsc /std:c++17 /O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX "/Fe:$ToolName.exe" @SourceFiles advapi32.lib user32.lib shell32.lib userenv.lib }
        'gcc'   { & g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$ToolName.exe" @SourceFiles -ladvapi32 -luser32 -lshell32 -luserenv }
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
