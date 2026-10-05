param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue killall.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[killall] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[killall] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[killall] Compiling with clang++..."
    & clang++ -std=c++17 -O2 killall.cpp -ladvapi32 -luser32 -Wl,/subsystem:console -o killall.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[killall] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[killall] Build successful: killall.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force killall.exe "..\..\bin\killall.exe"
            Write-Host "[killall] Installed to ..\..\bin\killall.exe"
        }
    }
}
finally {
    Pop-Location
}
