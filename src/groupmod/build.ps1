param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue groupmod.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[groupmod] Clean complete."
        return
    }
    if ($Action -notin @("build", "noinstall")) {
        throw "[groupmod] Unknown action '$Action'. Use 'build', 'noinstall', or 'clean'."
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[groupmod] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[groupmod] Compiling with clang++..."
    $sources = @(
        "groupmod.cpp", "groupmod_cli.cpp", "groupmod_group.cpp",
        "groupmod_input.cpp", "groupmod_app.cpp"
    )
    $arguments = @("-std=c++17", "-O2") + $sources +
        @("-lnetapi32", "-Wl,/subsystem:console", "-o", "groupmod.exe")
    & clang++ @arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[groupmod] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[groupmod] Build successful: groupmod.exe"

    if ($Action -ne "noinstall") {
        $binaryDirectory = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force $binaryDirectory | Out-Null
        Copy-Item -Force groupmod.exe (Join-Path $binaryDirectory "groupmod.exe")
        Write-Host "[groupmod] Installed to $binaryDirectory\groupmod.exe"
    }
}
finally {
    Pop-Location
}
