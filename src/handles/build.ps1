param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue handles.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[handles] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[handles] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[handles] Compiling with clang++..."
    & clang++ -std=c++17 -O2 handles.cpp -Wl,/subsystem:console -o handles.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[handles] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[handles] Build successful: handles.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force handles.exe "..\..\bin\handles.exe"
            Write-Host "[handles] Installed to ..\..\bin\handles.exe"
        }
    }
}
finally {
    Pop-Location
}
