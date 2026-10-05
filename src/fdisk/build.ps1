param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue fdisk.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[fdisk] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[fdisk] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[fdisk] Compiling with clang++..."
    & clang++ -std=c++17 -O2 fdisk.cpp -Wl,/subsystem:console -o fdisk.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[fdisk] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[fdisk] Build successful: fdisk.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force fdisk.exe "..\..\bin\fdisk.exe"
        }
    }
}
finally {
    Pop-Location
}
