param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue driverctl.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[driverctl] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[driverctl] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[driverctl] Compiling with clang++..."
    & clang++ -std=c++17 -O2 driverctl.cpp -Wl,/subsystem:console -o driverctl.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[driverctl] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[driverctl] Build successful: driverctl.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force driverctl.exe "..\..\bin\driverctl.exe"
        }
    }
}
finally {
    Pop-Location
}
