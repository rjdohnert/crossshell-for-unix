param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue winobj.exe, *.obj, *.pdb, *.ilk
        Write-Host "[winobj] Clean complete."
        return
    }

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        Write-Error "[winobj] Error: cl.exe not found in PATH. Run from a Visual Studio Developer PowerShell."
        exit 1
    }

    Write-Host "[winobj] Compiling with cl.exe..."
    & cl.exe /nologo /O2 /W4 /TC winobj.c /Fe:winobj.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[winobj] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[winobj] Build successful: winobj.exe"
    if ($Action -ne "noinstall") {
        $binDir = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force -Path $binDir | Out-Null
        Copy-Item -Force winobj.exe (Join-Path $binDir "winobj.exe")
        Write-Host "[winobj] Installed to $binDir\winobj.exe"
    }
}
finally {
    Pop-Location
}