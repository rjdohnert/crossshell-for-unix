param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue host.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[host] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[host] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[host] Compiling with clang++..."
    & clang++ -std=c++17 -O2 host.cpp -ldnsapi -lws2_32 -Wl,/subsystem:console -o host.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[host] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[host] Build successful: host.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force host.exe "..\..\bin\host.exe"
            Write-Host "[host] Installed to ..\..\bin\host.exe"
        }
    }
}
finally {
    Pop-Location
}
