param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue join.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[join] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[join] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[join] Compiling with clang++..."
    & clang++ -std=c++17 -O2 join.cpp -Wl,/subsystem:console -o join.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[join] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[join] Build successful: join.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force join.exe "..\..\bin\join.exe"
            Write-Host "[join] Installed to ..\..\bin\join.exe"
        }
    }
}
finally {
    Pop-Location
}
