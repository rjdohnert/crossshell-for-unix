param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue false.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[false] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[false] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[false] Compiling with clang++..."
    & clang++ -std=c++17 -O2 false.cpp -Wl,/subsystem:console -o false.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[false] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[false] Build successful: false.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force false.exe "..\..\bin\false.exe"
        }
    }
}
finally {
    Pop-Location
}
