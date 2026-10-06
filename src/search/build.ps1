<#
.SYNOPSIS
    Build script for search (Windows PowerShell)

.DESCRIPTION
    Compiles search using clang++, cl, or g++.
    Installs the resulting binary to ../../bin/search.exe.

.PARAMETER Compiler
    Explicitly choose compiler: 'clang', 'cl', 'gcc', or 'auto' (default: auto).

.PARAMETER Config
    Build configuration: 'Release' or 'Debug' (default: Release).

.PARAMETER Clean
    Removes build artifacts and binary.

.PARAMETER NoInstall
    Skips copying to bin directory.
#>

[CmdletBinding()]
param (
    [ValidateSet('auto', 'clang', 'cl', 'gcc')]
    [string]$Compiler = 'auto',

    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release',

    [switch]$Clean,
    [switch]$NoInstall
)

$ErrorActionPreference = 'Stop'
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $ScriptDir

$TargetName = "search.exe"
$Sources = @(
    "search.cpp",
    "terminal_format.cpp",
    "known_folders.cpp",
    "pattern_matcher.cpp",
    "search_options.cpp",
    "search_reporter.cpp",
    "search_engine.cpp"
)
$BinDir = Resolve-Path "$ScriptDir\..\..\bin" -ErrorAction SilentlyContinue

if ($Clean) {
    Write-Host "[search] Cleaning build artifacts..." -ForegroundColor Yellow
    Remove-Item -Path "*.obj", "*.o", "*.pdb", $TargetName -Force -ErrorAction SilentlyContinue
    Write-Host "[search] Clean complete." -ForegroundColor Green
    return
}

# Resolve compiler
$selectedCompiler = $null
if ($Compiler -eq 'auto') {
    if (Get-Command clang++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'clang' }
    elseif (Get-Command g++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'gcc' }
    elseif (Get-Command cl -ErrorAction SilentlyContinue) { $selectedCompiler = 'cl' }
    else {
        Write-Error "[search] Error: No suitable C++ compiler found (clang++, g++, cl.exe)."
        return
    }
} else {
    $selectedCompiler = $Compiler
}

Write-Host "[search] Building in $Config mode using $selectedCompiler..." -ForegroundColor Cyan

$LinkLibsClang = @("-lshell32", "-lole32")
$LinkLibsMsvc = @("Shell32.lib", "Ole32.lib")

if ($selectedCompiler -eq 'clang') {
    $cflags = if ($Config -eq 'Release') { @("-O2", "-DNDEBUG") } else { @("-g", "-O0", "-D_DEBUG") }
    & clang++ -std=c++17 @cflags @Sources @LinkLibsClang -o $TargetName
} elseif ($selectedCompiler -eq 'gcc') {
    $cflags = if ($Config -eq 'Release') { @("-O2", "-DNDEBUG") } else { @("-g", "-O0", "-D_DEBUG") }
    & g++ -std=c++17 @cflags @Sources @LinkLibsClang -o $TargetName
} elseif ($selectedCompiler -eq 'cl') {
    $cflags = if ($Config -eq 'Release') { @("/O2", "/DNDEBUG") } else { @("/Od", "/Zi", "/D_DEBUG") }
    & cl /nologo /EHsc /std:c++17 @cflags @Sources @LinkLibsMsvc /Fe:$TargetName
    Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path $TargetName)) {
    Write-Error "[search] Build failed: $TargetName was not produced."
    return
}

Write-Host "[search] Successfully built $TargetName" -ForegroundColor Green

if (-not $NoInstall) {
    if ($BinDir -and (Test-Path $BinDir)) {
        Copy-Item -Path $TargetName -Destination "$BinDir\$TargetName" -Force
        Write-Host "[search] Installed to $BinDir\$TargetName" -ForegroundColor Green
    } else {
        Write-Warning "[search] Bin directory not found, skipped installation."
    }
}
