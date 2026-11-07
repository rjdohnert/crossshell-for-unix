param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue dskctl.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[dskctl] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[dskctl] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[dskctl] Compiling with clang++..."
    $sources = @(
        "dskctl.cpp",
        "dskctl_app.cpp",
        "dskctl_engine.cpp",
        "dskctl_helpers.cpp",
        "dskctl_logger.cpp",
        "dskctl_options.cpp",
        "dskctl_reporter.cpp",
        "dskctl_security.cpp",
        "dskctl_targets.cpp"
    )
    & clang++ -std=c++17 -O2 @sources "-Wl,/subsystem:console" `
        -lshell32 -ladvapi32 -lole32 -o dskctl.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[dskctl] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[dskctl] Build successful: dskctl.exe"
    if ($Action -ne "noinstall") {
        $binDir = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force -ErrorAction Stop $binDir | Out-Null
        Copy-Item -Force -ErrorAction Stop dskctl.exe (Join-Path $binDir "dskctl.exe")
        if (-not (Test-Path (Join-Path $binDir "dskctl.exe"))) {
            throw "[dskctl] Error: failed to install dskctl.exe to bin\."
        }
    }
}
finally {
    Pop-Location
}
