param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue env.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[env] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[env] Error: clang++ not found in PATH."
        exit 1
    }

    $sources = @(
        "env.cpp",
        "env_arguments.cpp",
        "env_application.cpp",
        "env_environment.cpp",
        "env_output.cpp",
        "env_process.cpp"
    )
    Write-Host "[env] Compiling modules with clang++..."
    & clang++ -std=c++17 -O2 @sources "-Wl,/subsystem:console" -o env.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[env] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[env] Build successful: env.exe"
    if ($Action -ne "noinstall") {
        $binDir = Join-Path $scriptDir "..\..\bin"
        New-Item -ItemType Directory -Force -ErrorAction Stop $binDir | Out-Null
        $installedPath = Join-Path $binDir "env.exe"
        Copy-Item -Force -ErrorAction Stop env.exe $installedPath
        if (-not (Test-Path $installedPath)) {
            throw "[env] Error: failed to install env.exe to bin\."
        }
    }
}
finally {
    Pop-Location
}
