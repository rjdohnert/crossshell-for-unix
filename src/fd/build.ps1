param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue fd.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[fd] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[fd] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[fd] Compiling with clang++..."
    & clang++ -std=c++17 -O2 fd.cpp -Wl,/subsystem:console -o fd.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[fd] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[fd] Build successful: fd.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force fd.exe "..\..\bin\fd.exe"
        }
    }
}
finally {
    Pop-Location
}
