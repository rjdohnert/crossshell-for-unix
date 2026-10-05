param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue install.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[install] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[install] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[install] Compiling with clang++..."
    & clang++ -std=c++17 -O2 install.cpp -Wl,/subsystem:console -o install.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[install] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[install] Build successful: install.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force install.exe "..\..\bin\install.exe"
            Write-Host "[install] Installed to ..\..\bin\install.exe"
        }
    }
}
finally {
    Pop-Location
}
