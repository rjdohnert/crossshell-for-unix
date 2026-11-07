param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue gzip.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[gzip] Clean complete."
        return
    }
    if ($Action -ne "build") {
        throw "[gzip] Unknown action '$Action'. Use 'build' or 'clean'."
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[gzip] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[gzip] Compiling with clang++..."
    $sources = @(
        "gzip.cpp", "gzip_cli.cpp", "gzip_codec.cpp", "gzip_file.cpp",
        "gzip_member.cpp", "gzip_report.cpp", "gzip_engine.cpp", "gzip_app.cpp"
    )
    $arguments = @("-std=c++17", "-O2") + $sources +
        @("-Wl,/subsystem:console", "-o", "gzip.exe")
    & clang++ @arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[gzip] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[gzip] Build successful: gzip.exe"
    $binaryDirectory = Join-Path $scriptDir "..\..\bin"
    New-Item -ItemType Directory -Force $binaryDirectory | Out-Null
    Copy-Item -Force gzip.exe (Join-Path $binaryDirectory "gzip.exe")
    Write-Host "[gzip] Installed to $binaryDirectory\gzip.exe"
}
finally {
    Pop-Location
}
