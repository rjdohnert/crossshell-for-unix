param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue handles.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[handles] Clean complete."
        return
    }
    if ($Action -notin @("build", "noinstall")) {
        throw "[handles] Unknown action '$Action'. Use 'build', 'noinstall', or 'clean'."
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[handles] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[handles] Compiling with clang++..."
    $sources = @(
        "handles.cpp", "handles_cli.cpp", "handles_collect.cpp",
        "handles_format.cpp", "handles_app.cpp"
    )
    $arguments = @("-std=c++17", "-O2") + $sources +
        @("-Wl,/subsystem:console", "-o", "handles.exe")
    & clang++ @arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[handles] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[handles] Build successful: handles.exe"

    if ($Action -ne "noinstall") {
        $binaryDirectory = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force $binaryDirectory | Out-Null
        Copy-Item -Force handles.exe (Join-Path $binaryDirectory "handles.exe")
        Write-Host "[handles] Installed to $binaryDirectory\handles.exe"
    }
}
finally {
    Pop-Location
}
