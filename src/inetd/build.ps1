param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue inetd.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[inetd] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[inetd] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[inetd] Compiling with clang++..."
    & clang++ -std=c++17 -O2 inetd.cpp -lws2_32 -lshell32 -ladvapi32 -Wl,/subsystem:console -o inetd.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[inetd] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[inetd] Build successful: inetd.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force inetd.exe "..\..\bin\inetd.exe"
            Write-Host "[inetd] Installed to ..\..\bin\inetd.exe"
        }
    }
}
finally {
    Pop-Location
}
