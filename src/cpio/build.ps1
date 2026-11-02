[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl', 'gcc')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
$ToolName = 'cpio'
$SourceFiles = @(
    "cpio.c",
    "cpio_path.c",
    "cpio_inode.c",
    "cpio_format.c",
    "cpio_stream.c",
    "cpio_security.c",
    "cpio_engine.c",
    "cpio_cli.c"
)
Push-Location $PSScriptRoot
try {
    if ($Clean) {
        Remove-Item *.obj, *.o, "$ToolName.exe", *.pdb, *.ilk -Force -ErrorAction SilentlyContinue
        return
    }
    if ($Compiler -eq 'auto') {
        if (Get-Command clang -ErrorAction SilentlyContinue) { $Compiler = 'clang' }
        elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $Compiler = 'cl' }
        elseif (Get-Command gcc -ErrorAction SilentlyContinue) { $Compiler = 'gcc' }
        else { throw 'No suitable C compiler found (clang, cl, or gcc required).' }
    }
    switch ($Compiler) {
        'clang' { & clang -std=c99 -O2 -D_CRT_SECURE_NO_WARNINGS -o "$ToolName.exe" @SourceFiles -lshell32 }
        'cl'    { & cl.exe /nologo /std:c17 /O2 /D_CRT_SECURE_NO_WARNINGS "/Fe:$ToolName.exe" @SourceFiles shell32.lib }
        'gcc'   { & gcc -std=c99 -O2 -D_CRT_SECURE_NO_WARNINGS -o "$ToolName.exe" @SourceFiles -lshell32 }
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
