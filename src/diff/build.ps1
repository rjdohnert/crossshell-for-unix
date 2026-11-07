$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

$target = "diff"
$binDir = Join-Path $scriptDir "..\..\bin"
$src = "diff.c"

try {
    if (-not (Test-Path $binDir)) {
        New-Item -ItemType Directory -Path $binDir | Out-Null
    }

    $exePath = Join-Path $binDir "$target.exe"

    if (Get-Command clang -ErrorAction SilentlyContinue) {
        Write-Host "Building $target with clang..."
        & clang -O2 $src -o $exePath
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Build successful: $exePath"
            exit 0
        }
    }

    if (Get-Command clang++ -ErrorAction SilentlyContinue) {
        Write-Host "Building $target with clang++..."
        & clang++ -O2 $src -o $exePath
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Build successful: $exePath"
            exit 0
        }
    }

    if (Get-Command cl -ErrorAction SilentlyContinue) {
        Write-Host "Building $target with MSVC cl..."
        & cl /O2 /Fe:$exePath $src
        if ($LASTEXITCODE -eq 0) {
            Get-ChildItem -Filter "*.obj" | Remove-Item -Force
            Write-Host "Build successful: $exePath"
            exit 0
        }
    }

    if (Get-Command gcc -ErrorAction SilentlyContinue) {
        Write-Host "Building $target with gcc..."
        & gcc -O2 $src -o $exePath
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Build successful: $exePath"
            exit 0
        }
    }

    Write-Error "No suitable C compiler found (clang, cl, or gcc)."
    exit 1
}
finally {
    Pop-Location
}
