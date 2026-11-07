param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue fdisk.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[fdisk] Clean complete."
        return
    }

    if (-not (Get-Command clang -ErrorAction SilentlyContinue)) {
        Write-Error "[fdisk] Error: clang C compiler not found in PATH."
        exit 1
    }

    $sources = @(
        "fdisk.c",
        "fdisk_app.c",
        "fdisk_device.c",
        "fdisk_editor.c",
        "fdisk_error.c",
        "fdisk_format.c",
        "fdisk_image.c",
        "fdisk_options.c",
        "fdisk_volume.c"
    )
    Write-Host "[fdisk] Compiling C sources with clang..."
    & clang -std=c17 -O2 -Wall -Wextra -Wpedantic $sources "-Wl,/subsystem:console" -lsetupapi -lole32 -lvirtdisk -o fdisk.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[fdisk] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[fdisk] Build successful: fdisk.exe"
    if ($Action -ne "noinstall") {
        New-Item -ItemType Directory -Force "..\..\bin" | Out-Null
        Copy-Item -Force fdisk.exe "..\..\bin\fdisk.exe"
    }
}
finally {
    Pop-Location
}
