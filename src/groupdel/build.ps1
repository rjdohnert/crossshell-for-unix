param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue groupdel.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[groupdel] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[groupdel] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[groupdel] Compiling with clang++..."
    & clang++ -std=c++17 -O2 groupdel.cpp -lnetapi32 -Wl,/subsystem:console -o groupdel.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[groupdel] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[groupdel] Build successful: groupdel.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force groupdel.exe "..\..\bin\groupdel.exe"
            Write-Host "[groupdel] Installed to ..\..\bin\groupdel.exe"
        }
    }
}
finally {
    Pop-Location
}
