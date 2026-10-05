param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue ioscan.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[ioscan] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[ioscan] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[ioscan] Compiling with clang++..."
    & clang++ -std=c++17 -O2 ioscan.cpp -lsetupapi -lcfgmgr32 -Wl,/subsystem:console -o ioscan.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[ioscan] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[ioscan] Build successful: ioscan.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force ioscan.exe "..\..\bin\ioscan.exe"
            Write-Host "[ioscan] Installed to ..\..\bin\ioscan.exe"
        }
    }
}
finally {
    Pop-Location
}
