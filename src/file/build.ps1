param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue file.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[file] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[file] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[file] Compiling with clang++..."
    & clang++ -std=c++17 -O2 file.cpp -Wl,/subsystem:console -o file.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[file] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[file] Build successful: file.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force file.exe "..\..\bin\file.exe"
        }
    }
}
finally {
    Pop-Location
}
