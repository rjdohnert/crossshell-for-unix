param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue eve.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[eve] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[eve] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[eve] Compiling with clang++..."
    & clang++ -std=c++17 -O2 eve.cpp -Wl,/subsystem:console -o eve.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[eve] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[eve] Build successful: eve.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force eve.exe "..\..\bin\eve.exe"
        }
    }
}
finally {
    Pop-Location
}
