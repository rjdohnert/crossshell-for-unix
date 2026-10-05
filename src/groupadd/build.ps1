param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue groupadd.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[groupadd] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[groupadd] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[groupadd] Compiling with clang++..."
    & clang++ -std=c++17 -O2 groupadd.cpp -lnetapi32 -Wl,/subsystem:console -o groupadd.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[groupadd] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[groupadd] Build successful: groupadd.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force groupadd.exe "..\..\bin\groupadd.exe"
            Write-Host "[groupadd] Installed to ..\..\bin\groupadd.exe"
        }
    }
}
finally {
    Pop-Location
}
