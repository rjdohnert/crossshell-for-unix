param(
    [string]$Action = "build"
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue eve.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[eve] Clean complete."
        return
    }

    if (-not (Get-Command clang++ -ErrorAction SilentlyContinue)) {
        Write-Error "[eve] Error: clang++ not found in PATH."
        exit 1
    }

    $sources = @(
        "eve.cpp",
        "eve_app.cpp",
        "eve_console.cpp",
        "eve_editor.cpp",
        "eve_file_io.cpp",
        "eve_pipe.cpp"
    )
    Write-Host "[eve] Compiling modular sources with clang++..."
    & clang++ -std=c++17 -O2 -Wall -Wextra -Wpedantic $sources "-Wl,/subsystem:console" -o eve.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Error "[eve] Build failed."
        exit $LASTEXITCODE
    }

    Write-Host "[eve] Build successful: eve.exe"
    if ($Action -ne "noinstall") {
        try {
            New-Item -ItemType Directory -Force "..\..\bin" -ErrorAction Stop | Out-Null
            Copy-Item -Force eve.exe "..\..\bin\eve.exe" -ErrorAction Stop
        }
        catch {
            Write-Error "[eve] Installation to bin failed."
            exit 1
        }
        Write-Host "[eve] Installed: ..\..\bin\eve.exe"
    }
}
finally {
    Pop-Location
}
