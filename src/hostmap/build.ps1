param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue hostmap.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[hostmap] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[hostmap] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[hostmap] Compiling with clang++..."
    & clang++ -std=c++17 -O2 hostmap.cpp -lws2_32 -liphlpapi -ladvapi32 -Wl,/subsystem:console -o hostmap.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[hostmap] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[hostmap] Build successful: hostmap.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force hostmap.exe "..\..\bin\hostmap.exe"
            Write-Host "[hostmap] Installed to ..\..\bin\hostmap.exe"
        }
    }
}
finally {
    Pop-Location
}
