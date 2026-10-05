param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue getent.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[getent] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[getent] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[getent] Compiling with clang++..."
    & clang++ -std=c++17 -O2 getent.cpp -lnetapi32 -lws2_32 -liphlpapi -ladvapi32 -Wl,/subsystem:console -o getent.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[getent] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[getent] Build successful: getent.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force getent.exe "..\..\bin\getent.exe"
            Write-Host "[getent] Installed to ..\..\bin\getent.exe"
        }
    }
}
finally {
    Pop-Location
}
