param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue kill.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[kill] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[kill] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[kill] Compiling with clang++..."
    & clang++ -std=c++17 -O2 kill.cpp -ladvapi32 -luser32 -Wl,/subsystem:console -o kill.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[kill] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[kill] Build successful: kill.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force kill.exe "..\..\bin\kill.exe"
            Write-Host "[kill] Installed to ..\..\bin\kill.exe"
        }
    }
}
finally {
    Pop-Location
}
