param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue grep.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[grep] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[grep] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[grep] Compiling with clang++..."
    & clang++ -std=c++17 -O2 grep.cpp -lwbemuuid -lole32 -loleaut32 -ladvapi32 -Wl,/subsystem:console -o grep.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[grep] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[grep] Build successful: grep.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force grep.exe "..\..\bin\grep.exe"
            Write-Host "[grep] Installed to ..\..\bin\grep.exe"
        }
    }
}
finally {
    Pop-Location
}
