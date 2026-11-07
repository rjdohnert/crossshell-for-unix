param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue errpt.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[errpt] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[errpt] Error: clang++ not found in PATH."
        exit 1
    }

    $sources = @(
        "errpt.cpp",
        "errpt_application.cpp",
        "errpt_event_reader.cpp",
        "errpt_output.cpp",
        "errpt_parser.cpp"
    )
    Write-Host "[errpt] Compiling modules with clang++..."
    & clang++ -std=c++17 -O2 @sources "-Wl,/subsystem:console" -o errpt.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[errpt] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[errpt] Build successful: errpt.exe"
    if ($Action -ne "noinstall") {
        $binDir = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force -ErrorAction Stop $binDir | Out-Null
        $installedPath = Join-Path $binDir "errpt.exe"
        Copy-Item -Force -ErrorAction Stop errpt.exe $installedPath
        if (-not (Test-Path $installedPath)) {
            throw "[errpt] Error: failed to install errpt.exe to bin\."
        }
    }
}
finally {
    Pop-Location
}
