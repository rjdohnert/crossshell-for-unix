<#
.SYNOPSIS
    Build script for pkg (Windows PowerShell)

.DESCRIPTION
    Compiles pkg using clang++, cl, or g++.
    Installs the resulting binary to ../../bin/pkg.exe.

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

$TargetName = "pkg.exe"
$Sources = @("pkg.cpp", "pkg_app.cpp", "engine.cpp", "options.cpp")
$BinDir = Resolve-Path "$ScriptDir\..\..\bin" -ErrorAction SilentlyContinue

if ($Clean) {
    Write-Host "[pkg] Cleaning build artifacts..." -ForegroundColor Yellow
    Remove-Item -Path "*.obj", "*.o", "*.pdb", $TargetName -Force -ErrorAction SilentlyContinue
    Write-Host "[pkg] Clean complete." -ForegroundColor Green
    return
}

# Resolve compiler
$selectedCompiler = $null
if ($Compiler -eq 'auto') {
    if (Get-Command clang++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'clang' }
    elseif (Get-Command g++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'gcc' }
    elseif (Get-Command cl -ErrorAction SilentlyContinue) { $selectedCompiler = 'cl' }
    else {
        Write-Error "[pkg] Error: No suitable C++ compiler found (clang++, g++, cl.exe)."
        return
    }
} else {
    $selectedCompiler = $Compiler
}

Write-Host "[pkg] Building in $Config mode using $selectedCompiler..." -ForegroundColor Cyan

if ($selectedCompiler -eq 'clang') {
    $cflags = if ($Config -eq 'Release') { @("-O2", "-DNDEBUG") } else { @("-g", "-O0", "-D_DEBUG") }
    & clang++ -std=c++17 @cflags @Sources "-Wl,/subsystem:console" -o $TargetName
} elseif ($selectedCompiler -eq 'gcc') {
    $cflags = if ($Config -eq 'Release') { @("-O2", "-DNDEBUG") } else { @("-g", "-O0", "-D_DEBUG") }
    & g++ -std=c++17 @cflags -municode @Sources -o $TargetName
} elseif ($selectedCompiler -eq 'cl') {
    $cflags = if ($Config -eq 'Release') { @("/O2", "/DNDEBUG") } else { @("/Od", "/Zi", "/D_DEBUG") }
    & cl /nologo /EHsc /std:c++17 @cflags @Sources /Fe:$TargetName
    Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path $TargetName)) {
    Write-Error "[pkg] Build failed: $TargetName was not produced."
    return
}

Write-Host "[pkg] Successfully built $TargetName" -ForegroundColor Green

if (-not $NoInstall) {
    if ($BinDir -and (Test-Path $BinDir)) {
        Copy-Item -Path $TargetName -Destination "$BinDir\$TargetName" -Force
        Write-Host "[pkg] Installed to $BinDir\$TargetName" -ForegroundColor Green
    } else {
        Write-Warning "[pkg] bin directory not found. Binary kept at ./$TargetName"
    }
}
