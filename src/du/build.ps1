param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue du.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[du] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[du] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[du] Compiling with clang++..."
    & clang++ -std=c++17 -O2 du.cpp -Wl,/subsystem:console -o du.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[du] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[du] Build successful: du.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force du.exe "..\..\bin\du.exe"
        }
    }
}
finally {
    Pop-Location
}
