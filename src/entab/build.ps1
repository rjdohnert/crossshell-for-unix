param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue entab.exe, entab, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[entab] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[entab] Error: clang++ not found in PATH."
        exit 1
    }

    $sources = @(
        "entab.cpp",
        "entab_app.cpp",
        "entab_arg_parser.cpp",
        "entab_engine.cpp",
        "entab_help.cpp",
        "entab_reporter.cpp"
    )
    Write-Host "[entab] Compiling modules..."
    & clang++ -std=c++17 -O2 @sources "-Wl,/subsystem:console" -o entab.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[entab] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[entab] Build successful: entab.exe"
    if ($Action -ne "noinstall") {
        $binDir = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force -ErrorAction Stop $binDir | Out-Null
        $installedPath = Join-Path $binDir "entab.exe"
        Copy-Item -Force -ErrorAction Stop entab.exe $installedPath
        if (-not (Test-Path $installedPath)) {
            throw "[entab] Error: failed to install entab.exe to bin\."
        }
    }
}
finally {
    Pop-Location
}
