<#
.SYNOPSIS
    Build script for setboot (Windows PowerShell)

.DESCRIPTION
    Compiles setboot using clang++, cl, or g++.
    Installs the resulting binary to ../../bin/setboot.exe.

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

$TargetName = "setboot.exe"
$Sources = @(
    "setboot.cpp",
    "setboot_options.cpp",
    "nvram_controller.cpp",
    "boot_reporter.cpp"
)
$BinDir = Resolve-Path "$ScriptDir\..\..\bin" -ErrorAction SilentlyContinue

if ($Clean) {
    Write-Host "[setboot] Cleaning build artifacts..." -ForegroundColor Yellow
    Remove-Item -Path "*.obj", "*.o", "*.pdb", $TargetName -Force -ErrorAction SilentlyContinue
    Write-Host "[setboot] Clean complete." -ForegroundColor Green
    return
}

# Resolve compiler
$selectedCompiler = $null
if ($Compiler -eq 'auto') {
    if (Get-Command clang++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'clang' }
    elseif (Get-Command g++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'gcc' }
    elseif (Get-Command cl -ErrorAction SilentlyContinue) { $selectedCompiler = 'cl' }
    else {
        Write-Error "[setboot] Error: No suitable C++ compiler found (clang++, g++, cl.exe)."
        return
    }
} else {
    $selectedCompiler = $Compiler
}

Write-Host "[setboot] Building in $Config mode using $selectedCompiler..." -ForegroundColor Cyan

$LinkLibsClang = @("-ladvapi32")
$LinkLibsMsvc = @("advapi32.lib")

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
    Write-Error "[setboot] Build failed: $TargetName was not produced."
    return
}

Write-Host "[setboot] Successfully built $TargetName" -ForegroundColor Green

if (-not $NoInstall) {
    if ($BinDir -and (Test-Path $BinDir)) {
        Copy-Item -Path $TargetName -Destination "$BinDir\$TargetName" -Force
        Write-Host "[setboot] Installed to $BinDir\$TargetName" -ForegroundColor Green
    } else {
        Write-Warning "[setboot] Bin directory not found, skipped installation."
    }
}
