param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue groupadd.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[groupadd] Clean complete."
        return
    }
    if ($Action -notin @("build", "noinstall")) {
        throw "[groupadd] Unknown action '$Action'. Use 'build', 'noinstall', or 'clean'."
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[groupadd] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[groupadd] Compiling with clang++..."
    $sources = @(
        "groupadd.cpp", "groupadd_cli.cpp", "groupadd_group.cpp",
        "groupadd_input.cpp", "groupadd_app.cpp"
    )
    $arguments = @("-std=c++17", "-O2") + $sources +
        @("-lnetapi32", "-Wl,/subsystem:console", "-o", "groupadd.exe")
    & clang++ @arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[groupadd] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[groupadd] Build successful: groupadd.exe"

    if ($Action -ne "noinstall") {
        $binaryDirectory = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force $binaryDirectory | Out-Null
        Copy-Item -Force groupadd.exe (Join-Path $binaryDirectory "groupadd.exe")
        Write-Host "[groupadd] Installed to $binaryDirectory\groupadd.exe"
    }
}
finally {
    Pop-Location
}
