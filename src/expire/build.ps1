param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue expire.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[expire] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[expire] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[expire] Compiling with clang++..."
    & clang++ -std=c++17 -O2 expire.cpp -Wl,/subsystem:console -o expire.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[expire] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[expire] Build successful: expire.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force expire.exe "..\..\bin\expire.exe"
        }
    }
}
finally {
    Pop-Location
}
