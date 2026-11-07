param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue htop.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[htop] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[htop] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[htop] Compiling with clang++..."
    & clang++ -std=c++17 -O2 htop.cpp htop_app.cpp htop_telemetry.cpp htop_console.cpp htop_format.cpp -ladvapi32 -lpsapi -lntdll '-Wl,/subsystem:console' -o htop.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[htop] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[htop] Build successful: htop.exe"

    if ($Action -ne "noinstall") {
        $binDirectory = Join-Path $scriptDir "..\..\bin"
        $null = New-Item -ItemType Directory -Path $binDirectory -Force
        Copy-Item -Force htop.exe (Join-Path $binDirectory "htop.exe")
        Write-Host "[htop] Installed to $binDirectory\htop.exe"
    }
}
finally {
    Pop-Location
}
