param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue errpt.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[errpt] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[errpt] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[errpt] Compiling with clang++..."
    & clang++ -std=c++17 -O2 errpt.cpp -Wl,/subsystem:console -o errpt.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[errpt] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[errpt] Build successful: errpt.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force errpt.exe "..\..\bin\errpt.exe"
        }
    }
}
finally {
    Pop-Location
}
