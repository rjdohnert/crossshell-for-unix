param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue kgdb.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[kgdb] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[kgdb] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[kgdb] Compiling with clang++..."
    & clang++ -std=c++17 -O2 kgdb.cpp -lws2_32 -lpsapi -Wl,/subsystem:console -o kgdb.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[kgdb] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[kgdb] Build successful: kgdb.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force kgdb.exe "..\..\bin\kgdb.exe"
            Write-Host "[kgdb] Installed to ..\..\bin\kgdb.exe"
        }
    }
}
finally {
    Pop-Location
}
