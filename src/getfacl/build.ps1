param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue getfacl.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[getfacl] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[getfacl] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[getfacl] Compiling with clang++..."
    & clang++ -std=c++17 -O2 getfacl.cpp -ladvapi32 -Wl,/subsystem:console -o getfacl.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[getfacl] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[getfacl] Build successful: getfacl.exe"

    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force getfacl.exe "..\..\bin\getfacl.exe"
            Write-Host "[getfacl] Installed to ..\..\bin\getfacl.exe"
        }
    }
}
finally {
    Pop-Location
}
