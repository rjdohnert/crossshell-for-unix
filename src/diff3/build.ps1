$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

$target = "diff3"
$binDir = Join-Path $scriptDir "..\..\bin"

if (Test-Path "diff3.c") {
    $src = @("diff3.c")
} elseif (Test-Path "diff3_app.cpp") {
    $src = @("diff3.cpp", "diff3_app.cpp", "arg_parser.cpp", "formatter.cpp", "diff3_engine.cpp", "myers_diff.cpp", "line_reader.cpp")
} else {
    $src = @("diff3.cpp")
}

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
        & clang++ -std=c++17 -O2 $src -o $exePath
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Build successful: $exePath"
            exit 0
        }
    }

    if (Get-Command cl -ErrorAction SilentlyContinue) {
        Write-Host "Building $target with MSVC cl..."
        & cl /std:c++17 /O2 /EHsc /Fe:$exePath $src
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

    if (Get-Command g++ -ErrorAction SilentlyContinue) {
        Write-Host "Building $target with g++..."
        & g++ -std=c++17 -O2 $src -o $exePath
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Build successful: $exePath"
            exit 0
        }
    }

    Write-Error "No suitable compiler found (clang, clang++, cl, gcc, or g++)."
    exit 1
}
finally {
    Pop-Location
}
