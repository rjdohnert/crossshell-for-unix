param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue fsctl.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[fsctl] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[fsctl] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[fsctl] Compiling with clang++..."
    & clang++ -std=c++17 -O2 fsctl.cpp -lmpr -lwininet -lshell32 -ladvapi32 -Wl,/subsystem:console -o fsctl.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[fsctl] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[fsctl] Build successful: fsctl.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force fsctl.exe "..\..\bin\fsctl.exe"
            Write-Host "[fsctl] Installed to ..\..\bin\fsctl.exe"
        }
    }
}
finally {
    Pop-Location
}
