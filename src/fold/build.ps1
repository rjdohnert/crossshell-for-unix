param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue fold.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[fold] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[fold] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[fold] Compiling with clang++..."
    & clang++ -std=c++17 -O2 fold.cpp -Wl,/subsystem:console -o fold.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[fold] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[fold] Build successful: fold.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force fold.exe "..\..\bin\fold.exe"
            Write-Host "[fold] Installed to ..\..\bin\fold.exe"
        }
    }
}
finally {
    Pop-Location
}
