param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue entab.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[entab] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[entab] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[entab] Compiling with clang++..."
    & clang++ -std=c++17 -O2 entab.cpp -Wl,/subsystem:console -o entab.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[entab] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[entab] Build successful: entab.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force entab.exe "..\..\bin\entab.exe"
        }
    }
}
finally {
    Pop-Location
}
