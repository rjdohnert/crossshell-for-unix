[CmdletBinding()]
param(
    [switch]$Clean,
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location $ScriptDir

if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    Remove-Item -Force -ErrorAction SilentlyContinue *.obj, *.o, *.exe, *.pdb, *.ilk
    Write-Host "Clean complete." -ForegroundColor Green
    exit 0
}

Write-Host "Building dos2unix and unix2dos ($Configuration)..." -ForegroundColor Cyan

$sources = @(
    "dos2unix.cpp",
    "app.cpp",
    "converter.cpp",
    "options.cpp",
    "reporter.cpp",
    "timestamp_helper.cpp"
)

$cl = Get-Command "cl.exe" -ErrorAction SilentlyContinue
$vcc = Get-Command "vcc.exe" -ErrorAction SilentlyContinue
$clang = Get-Command "clang++.exe" -ErrorAction SilentlyContinue
$gcc = Get-Command "g++.exe" -ErrorAction SilentlyContinue

if ($cl) {
    Write-Host "Using MSVC compiler (cl.exe)..." -ForegroundColor Green
    $opt = if ($Configuration -eq "Debug") { @("/Od", "/Zi", "/DEBUG") } else { @("/O2") }
    $cmdArgs = @("/nologo", "/std:c++17", "/EHsc", "/W4", "/permissive-", "/utf-8", "/Fe:dos2unix.exe") + $opt + $sources + @("Shell32.lib")
    & $cl.Source $cmdArgs
    if ($LASTEXITCODE -ne 0) { throw "MSVC build failed with code $LASTEXITCODE" }
} elseif ($vcc) {
    Write-Host "Using vcc compiler..." -ForegroundColor Green
    $opt = if ($Configuration -eq "Debug") { @("-g", "-O0") } else { @("-O2") }
    $cmdArgs = @("-std=c++17") + $opt + @("-o", "dos2unix.exe") + $sources + @("-lshell32")
    & $vcc.Source $cmdArgs
    if ($LASTEXITCODE -ne 0) { throw "vcc build failed with code $LASTEXITCODE" }
} elseif ($clang) {
    Write-Host "Using clang++ compiler..." -ForegroundColor Green
    $opt = if ($Configuration -eq "Debug") { @("-g", "-O0") } else { @("-O2") }
    $cmdArgs = @("-std=c++17", "-municode") + $opt + @("-o", "dos2unix.exe") + $sources + @("-lshell32")
    & $clang.Source $cmdArgs
    if ($LASTEXITCODE -ne 0) { throw "clang++ build failed with code $LASTEXITCODE" }
} elseif ($gcc) {
    Write-Host "Using g++ compiler..." -ForegroundColor Green
    $opt = if ($Configuration -eq "Debug") { @("-g", "-O0") } else { @("-O2") }
    $cmdArgs = @("-std=c++17", "-municode") + $opt + @("-o", "dos2unix.exe") + $sources + @("-lshell32")
    & $gcc.Source $cmdArgs
    if ($LASTEXITCODE -ne 0) { throw "g++ build failed with code $LASTEXITCODE" }
} else {
    throw "No supported C++ compiler found (cl.exe, vcc, clang++, or g++). Please run from a Visual Studio Developer Command Prompt or ensure a compiler is in your PATH."
}

Copy-Item -Force "dos2unix.exe" "unix2dos.exe"
if (Test-Path "..\..\bin") {
    Copy-Item -Force "dos2unix.exe" "..\..\bin\dos2unix.exe"
    Copy-Item -Force "unix2dos.exe" "..\..\bin\unix2dos.exe"
    Write-Host "Installed dos2unix.exe and unix2dos.exe to ..\..\bin\" -ForegroundColor Green
}
Write-Host "Build succeeded: dos2unix.exe and unix2dos.exe created." -ForegroundColor Green
