param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue iconv.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[iconv] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[iconv] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[iconv] Compiling with clang++..."
    & clang++ -std=c++17 -O2 iconv.cpp -Wl,/subsystem:console -o iconv.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[iconv] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[iconv] Build successful: iconv.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force iconv.exe "..\..\bin\iconv.exe"
            Write-Host "[iconv] Installed to ..\..\bin\iconv.exe"
        }
    }
}
finally {
    Pop-Location
}
