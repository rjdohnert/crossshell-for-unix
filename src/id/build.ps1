param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue id.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[id] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[id] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[id] Compiling with clang++..."
    & clang++ -std=c++17 -O2 id.cpp -ladvapi32 -lnetapi32 -Wl,/subsystem:console -o id.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[id] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[id] Build successful: id.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force id.exe "..\..\bin\id.exe"
            Write-Host "[id] Installed to ..\..\bin\id.exe"
        }
    }
}
finally {
    Pop-Location
}
