param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue hostid.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[hostid] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[hostid] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[hostid] Compiling with clang++..."
    & clang++ -std=c++17 -O2 hostid.cpp -lws2_32 -ladvapi32 -Wl,/subsystem:console -o hostid.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[hostid] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[hostid] Build successful: hostid.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force hostid.exe "..\..\bin\hostid.exe"
            Write-Host "[hostid] Installed to ..\..\bin\hostid.exe"
        }
    }
}
finally {
    Pop-Location
}
