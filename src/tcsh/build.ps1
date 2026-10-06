<#
.SYNOPSIS
    Build script for tcsh (Windows PowerShell)

.DESCRIPTION
    Compiles tcsh using clang++, cl, or g++.
    Installs the resulting binary to ../../bin/tcsh.exe.

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

$TargetName = "tcsh.exe"
$Sources = @(
    "tcsh.cpp",
    "tcsh_app.cpp",
    "options.cpp",
    "engine.cpp",
    "parser.cpp",
    "expansion.cpp",
    "builtins.cpp",
    "jobs.cpp",
    "terminal.cpp",
    "scripting.cpp",
    "selftest.cpp"
)
$BinDir = Resolve-Path "$ScriptDir\..\..\bin" -ErrorAction SilentlyContinue

if ($Clean) {
    Write-Host "[tcsh] Cleaning build artifacts..." -ForegroundColor Yellow
    Remove-Item -Path "*.obj", "*.o", "*.pdb", $TargetName -Force -ErrorAction SilentlyContinue
    Write-Host "[tcsh] Clean complete." -ForegroundColor Green
    return
}

# Resolve compiler
$selectedCompiler = $null
if ($Compiler -eq 'auto') {
    if (Get-Command clang++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'clang' }
    elseif (Get-Command g++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'gcc' }
    elseif (Get-Command cl -ErrorAction SilentlyContinue) { $selectedCompiler = 'cl' }
    else {
        Write-Error "[tcsh] Error: No suitable C++ compiler found (clang++, g++, cl.exe)."
        return
    }
} else {
    $selectedCompiler = $Compiler
}

Write-Host "[tcsh] Building in $Config mode using $selectedCompiler..." -ForegroundColor Cyan

$LinkLibsClang = @("-lshell32", "-luser32", "-ladvapi32")
$LinkLibsMsvc = @("shell32.lib", "user32.lib", "advapi32.lib")

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
    Write-Error "[tcsh] Build failed: $TargetName was not produced."
    return
}

Write-Host "[tcsh] Successfully built $TargetName" -ForegroundColor Green

if (-not $NoInstall) {
    if ($BinDir -and (Test-Path $BinDir)) {
        Copy-Item -Path $TargetName -Destination "$BinDir\$TargetName" -Force
        Write-Host "[tcsh] Installed to $BinDir\$TargetName" -ForegroundColor Green
    } else {
        Write-Warning "[tcsh] bin directory not found. Binary kept at ./$TargetName"
    }
}
