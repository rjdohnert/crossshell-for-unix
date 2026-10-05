param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue groups.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[groups] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[groups] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[groups] Compiling with clang++..."
    & clang++ -std=c++17 -O2 groups.cpp -lnetapi32 -ladvapi32 -Wl,/subsystem:console -o groups.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[groups] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[groups] Build successful: groups.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force groups.exe "..\..\bin\groups.exe"
            Write-Host "[groups] Installed to ..\..\bin\groups.exe"
        }
    }
}
finally {
    Pop-Location
}
