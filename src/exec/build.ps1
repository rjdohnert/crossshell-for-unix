param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue exec.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[exec] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[exec] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[exec] Compiling with clang++..."
    & clang++ -std=c++17 -O2 exec.cpp -Wl,/subsystem:console -o exec.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[exec] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[exec] Build successful: exec.exe"
    if ($Action -ne "noinstall") {
        if (Test-Path "..\..\bin") {
            Copy-Item -Force exec.exe "..\..\bin\exec.exe"
        }
    }
}
finally {
    Pop-Location
}
