[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl')][string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)

$ErrorActionPreference = 'Stop'
$sourceFiles = @('ls.c', 'security.c', 'files.c', 'scan.c', 'listing.c')
Push-Location $PSScriptRoot
try {
    if ($Clean) {
        Remove-Item ls.exe, *.obj, *.o, *.pdb, *.ilk -Force -ErrorAction SilentlyContinue
        return
    }

    if ($Compiler -eq 'auto') {
        if (Get-Command clang -ErrorAction SilentlyContinue) { $Compiler = 'clang' }
        elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $Compiler = 'cl' }
        else { throw 'A Windows C compiler is required: clang or cl.exe.' }
    }

    if ($Compiler -eq 'clang') {
        & clang -std=c17 -O2 -Wall -Wextra @sourceFiles -ladvapi32 -o ls.exe
    } else {
           & cl.exe /nologo /std:c17 /TC /O2 /W4 "/Fe:ls.exe" @sourceFiles /link Advapi32.lib
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed for ls (exit $LASTEXITCODE)." }

    if (-not $NoInstall) {
        $binDir = Join-Path $PSScriptRoot '../../bin'
        New-Item -ItemType Directory $binDir -Force | Out-Null
        Copy-Item ls.exe (Join-Path $binDir 'ls.exe') -Force
    }
} finally {
    Pop-Location
}