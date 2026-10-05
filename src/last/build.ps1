param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue last.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[last] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[last] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[last] Compiling with clang++..."
    & clang++ -std=c++17 -O2 last.cpp -lwevtapi -ladvapi32 -Wl,/subsystem:console -o last.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[last] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[last] Build successful: last.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force last.exe "..\..\bin\last.exe"
            Write-Host "[last] Installed to ..\..\bin\last.exe"
        }
    }
}
finally {
    Pop-Location
}
