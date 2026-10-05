param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue head.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[head] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[head] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[head] Compiling with clang++..."
    & clang++ -std=c++17 -O2 head.cpp -Wl,/subsystem:console -o head.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[head] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[head] Build successful: head.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force head.exe "..\..\bin\head.exe"
            Write-Host "[head] Installed to ..\..\bin\head.exe"
        }
    }
}
finally {
    Pop-Location
}
