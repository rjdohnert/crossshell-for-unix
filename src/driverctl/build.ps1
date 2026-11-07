param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue driverctl.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[driverctl] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[driverctl] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[driverctl] Compiling with clang++..."
    $sources = @(
        "driverctl.cpp",
        "driverctl_app.cpp",
        "driverctl_scanner.cpp",
        "driverctl_logger.cpp",
        "driverctl_decoder.cpp",
        "driverctl_formatters.cpp",
        "driverctl_utilities.cpp"
    )
    & clang++ -std=c++17 -O2 @sources "-Wl,/subsystem:console" `
        -lsetupapi -lcfgmgr32 -ladvapi32 -lshell32 -lole32 -o driverctl.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[driverctl] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[driverctl] Build successful: driverctl.exe"
    if ($Action -ne "noinstall") {
        $binDir = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force -ErrorAction Stop $binDir | Out-Null
        Copy-Item -Force -ErrorAction Stop driverctl.exe (Join-Path $binDir "driverctl.exe")
        if (-not (Test-Path (Join-Path $binDir "driverctl.exe"))) {
            throw "[driverctl] Error: failed to install driverctl.exe to bin\."
        }
    }
}
finally {
    Pop-Location
}
