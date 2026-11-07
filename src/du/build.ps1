param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue du.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[du] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[du] Error: clang++ not found in PATH."
        exit 1
    }

    $sources = @(
        "du.cpp",
        "du_app.cpp",
        "du_arg_parser.cpp",
        "du_help_system.cpp",
        "du_input_stream_reader.cpp",
        "du_path_filter.cpp",
        "du_reporter.cpp",
        "du_scanner.cpp",
        "du_size_formatter.cpp"
    )
    Write-Host "[du] Compiling modules..."
    & clang++ -std=c++17 -O2 @sources "-Wl,/subsystem:console" -o du.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[du] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[du] Build successful: du.exe"
    if ($Action -ne "noinstall") {
        $binDir = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force -ErrorAction Stop $binDir | Out-Null
        $installedPath = Join-Path $binDir "du.exe"
        Copy-Item -Force -ErrorAction Stop du.exe $installedPath
        if (-not (Test-Path $installedPath)) {
            throw "[du] Error: failed to install du.exe to bin\."
        }
    }
}
finally {
    Pop-Location
}
