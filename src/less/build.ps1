param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue less.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[less] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[less] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[less] Compiling with clang++..."
    & clang++ -std=c++17 -O2 less.cpp -Wl,/subsystem:console -o less.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[less] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[less] Build successful: less.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force less.exe "..\..\bin\less.exe"
            Write-Host "[less] Installed to ..\..\bin\less.exe"
        }
    }
}
finally {
    Pop-Location
}
