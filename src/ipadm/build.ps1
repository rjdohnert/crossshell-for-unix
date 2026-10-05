param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue ipadm.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[ipadm] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[ipadm] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[ipadm] Compiling with clang++..."
    & clang++ -std=c++17 -O2 ipadm.cpp -liphlpapi -lws2_32 -Wl,/subsystem:console -o ipadm.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[ipadm] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[ipadm] Build successful: ipadm.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force ipadm.exe "..\..\bin\ipadm.exe"
            Write-Host "[ipadm] Installed to ..\..\bin\ipadm.exe"
        }
    }
}
finally {
    Pop-Location
}
