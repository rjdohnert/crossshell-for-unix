param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue findmnt.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[findmnt] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[findmnt] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[findmnt] Compiling with clang++..."
    & clang++ -std=c++17 -O2 findmnt.cpp -lmpr -Wl,/subsystem:console -o findmnt.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[findmnt] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[findmnt] Build successful: findmnt.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force findmnt.exe "..\..\bin\findmnt.exe"
            Write-Host "[findmnt] Installed to ..\..\bin\findmnt.exe"
        }
    }
}
finally {
    Pop-Location
}
