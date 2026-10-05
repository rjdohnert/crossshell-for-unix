param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue groupmod.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[groupmod] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[groupmod] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[groupmod] Compiling with clang++..."
    & clang++ -std=c++17 -O2 groupmod.cpp -lnetapi32 -Wl,/subsystem:console -o groupmod.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[groupmod] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[groupmod] Build successful: groupmod.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force groupmod.exe "..\..\bin\groupmod.exe"
            Write-Host "[groupmod] Installed to ..\..\bin\groupmod.exe"
        }
    }
}
finally {
    Pop-Location
}
