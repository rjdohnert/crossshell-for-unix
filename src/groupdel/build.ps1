param(
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue groupdel.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[groupdel] Clean complete."
        return
    }
    if ($Action -notin @("build", "noinstall")) {
        throw "[groupdel] Unknown action '$Action'. Use 'build', 'noinstall', or 'clean'."
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[groupdel] Error: clang++ not found in PATH."
        exit 1
    }

    Write-Host "[groupdel] Compiling with clang++..."
    $sources = @(
        "groupdel.cpp", "groupdel_cli.cpp", "groupdel_group.cpp",
        "groupdel_input.cpp", "groupdel_app.cpp"
    )
    $arguments = @("-std=c++17", "-O2") + $sources +
        @("-lnetapi32", "-Wl,/subsystem:console", "-o", "groupdel.exe")
    & clang++ @arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[groupdel] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[groupdel] Build successful: groupdel.exe"

    if ($Action -ne "noinstall") {
        $binaryDirectory = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force $binaryDirectory | Out-Null
        Copy-Item -Force groupdel.exe (Join-Path $binaryDirectory "groupdel.exe")
        Write-Host "[groupdel] Installed to $binaryDirectory\groupdel.exe"
    }
}
finally {
    Pop-Location
}
