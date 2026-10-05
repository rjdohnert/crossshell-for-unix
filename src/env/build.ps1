param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue env.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[env] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[env] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[env] Compiling with clang++..."
    & clang++ -std=c++17 -O2 env.cpp -Wl,/subsystem:console -o env.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[env] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[env] Build successful: env.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force env.exe "..\..\bin\env.exe"
        }
    }
}
finally {
    Pop-Location
}
