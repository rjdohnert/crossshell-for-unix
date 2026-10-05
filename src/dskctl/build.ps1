param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue dskctl.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[dskctl] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[dskctl] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[dskctl] Compiling with clang++..."
    & clang++ -std=c++17 -O2 dskctl.cpp -Wl,/subsystem:console -o dskctl.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[dskctl] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[dskctl] Build successful: dskctl.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force dskctl.exe "..\..\bin\dskctl.exe"
        }
    }
}
finally {
    Pop-Location
}
