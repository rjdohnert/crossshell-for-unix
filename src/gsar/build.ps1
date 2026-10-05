param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue gsar.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[gsar] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[gsar] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[gsar] Compiling with clang++..."
    & clang++ -std=c++17 -O2 gsar.cpp -Wl,/subsystem:console -o gsar.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[gsar] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[gsar] Build successful: gsar.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force gsar.exe "..\..\bin\gsar.exe"
            Write-Host "[gsar] Installed to ..\..\bin\gsar.exe"
        }
    }
}
finally {
    Pop-Location
}
