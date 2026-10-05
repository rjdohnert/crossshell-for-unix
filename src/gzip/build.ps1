param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue gzip.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[gzip] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[gzip] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[gzip] Compiling with clang++..."
    & clang++ -std=c++17 -O2 gzip.cpp -Wl,/subsystem:console -o gzip.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[gzip] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[gzip] Build successful: gzip.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force gzip.exe "..\..\bin\gzip.exe"
            Write-Host "[gzip] Installed to ..\..\bin\gzip.exe"
        }
    }
}
finally {
    Pop-Location
}
