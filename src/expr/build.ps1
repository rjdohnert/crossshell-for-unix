param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue expr.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[expr] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[expr] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[expr] Compiling with clang++..."
    & clang++ -std=c++17 -O2 expr.cpp -Wl,/subsystem:console -o expr.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[expr] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[expr] Build successful: expr.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force expr.exe "..\..\bin\expr.exe"
        }
    }
}
finally {
    Pop-Location
}
