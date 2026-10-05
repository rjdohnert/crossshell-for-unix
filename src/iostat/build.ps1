param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue iostat.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[iostat] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[iostat] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[iostat] Compiling with clang++..."
    & clang++ -std=c++17 -O2 iostat.cpp -lpdh -Wl,/subsystem:console -o iostat.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[iostat] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[iostat] Build successful: iostat.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force iostat.exe "..\..\bin\iostat.exe"
            Write-Host "[iostat] Installed to ..\..\bin\iostat.exe"
        }
    }
}
finally {
    Pop-Location
}
