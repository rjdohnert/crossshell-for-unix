# CrossShell for UNIX - zstd Build Script (PowerShell)
[CmdletBinding()]
param(
    [string]$Compiler = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = (Resolve-Path (Join-Path $ScriptDir "..\..")).Path
$BinDir = Join-Path $RootDir "bin"
$Target = Join-Path $BinDir "zstd.exe"

if (-not (Test-Path $BinDir)) {
    New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
}

$Sources = @("zstd.c", "xxh64.c", "codec.c", "pool.c", "engine.c", "zstddeclib.c") | ForEach-Object { Join-Path $ScriptDir $_ }

Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "Building zstd - CrossShell RFC 8878 Enterprise" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan

# Autodetect compiler
if (-not $Compiler) {
    if (Get-Command clang.exe -ErrorAction SilentlyContinue) { $Compiler = "clang" }
    elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $Compiler = "cl" }
    elseif (Get-Command gcc.exe -ErrorAction SilentlyContinue) { $Compiler = "gcc" }
    else {
        Write-Error "No supported C compiler found (clang, cl, gcc)."
        exit 1
    }
}

switch ($Compiler.ToLower()) {
    "clang" {
        Write-Host "Compiling with Clang -O3..." -ForegroundColor Green
        & clang -O3 -Wall -Wextra -Wno-unused-function -Wno-unused-const-variable $Sources -o $Target
    }
    "cl" {
        Write-Host "Compiling with MSVC cl /O2..." -ForegroundColor Green
        & cl /O2 /W3 /D_CRT_SECURE_NO_WARNINGS $Sources "/Fe:$Target" /link
    }
    "gcc" {
        Write-Host "Compiling with GCC -O3..." -ForegroundColor Green
        & gcc -O3 -Wall -Wextra $Sources -o $Target
    }
    default {
        Write-Error "Unsupported compiler: $Compiler"
        exit 1
    }
}

if ($LASTEXITCODE -eq 0 -and (Test-Path $Target)) {
    $info = Get-Item $Target
    Write-Host "[SUCCESS] Built $($info.FullName) ($($info.Length) bytes)" -ForegroundColor Green
    exit 0
} else {
    Write-Error "[FAILURE] Compilation failed."
    exit 1
}
