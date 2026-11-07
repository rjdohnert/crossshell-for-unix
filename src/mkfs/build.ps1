param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue mkfs.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[mkfs] Clean complete."
        return
    }

    if (-not (Get-Command clang -ErrorAction SilentlyContinue)) {
        Write-Error "[mkfs] Error: clang C compiler not found in PATH."
        exit 1
    }

    $sources = @(
        "mkfs.c",
        "mkfs_cli.c",
        "mkfs_engine.c",
        "mkfs_error.c",
        "mkfs_fmifs.c"
    )
    Write-Host "[mkfs] Compiling C sources with clang..."
    & clang -std=c17 -O2 -Wall -Wextra -Wpedantic $sources "-Wl,/subsystem:console" -o mkfs.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[mkfs] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[mkfs] Build successful: mkfs.exe"
    if ($Action -ne "noinstall") {
        New-Item -ItemType Directory -Force "..\..\bin" | Out-Null
        Copy-Item -Force mkfs.exe "..\..\bin\mkfs.exe"
        Write-Host "[mkfs] Installed to bin\mkfs.exe"
    }
}
finally {
    Pop-Location
}
