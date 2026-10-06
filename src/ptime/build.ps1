<#
 .Synopsis
    Builds the ptime utility using Clang++, MSVC (cl), or GCC (g++).
 .Description
    Automates compiling ptime with options to clean build artifacts,
    select compiler toolchains, and install the final binary to the project bin directory.
 .Parameter Compiler
    Specifies the compiler to use: 'clang', 'cl', or 'gcc'. Default is 'clang'.
 .Parameter Clean
    Removes temporary build artifacts (.obj, .o, etc.).
 .Parameter NoInstall
    Skips copying the binary to the project's bin directory.
#>
[CmdletBinding()]
param(
    [ValidateSet('clang', 'cl', 'gcc', 'auto')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)

$ErrorActionPreference = 'Stop'

$ToolName = "ptime"
$SourceFiles = @("ptime.cpp", "options.cpp", "engine.cpp", "reporter.cpp", "ptime_app.cpp")
$BinDir = Resolve-Path "$PSScriptRoot\..\..\bin" -ErrorAction SilentlyContinue

if ($Clean) {
    Write-Host "Cleaning build artifacts for $ToolName..." -ForegroundColor Cyan
    Remove-Item -Path "$PSScriptRoot\*.obj", "$PSScriptRoot\*.o", "$PSScriptRoot\*.exe", "$PSScriptRoot\*.pdb", "$PSScriptRoot\*.ilk" -Force -ErrorAction SilentlyContinue
    Write-Host "Clean completed." -ForegroundColor Green
    return
}

# Auto-detect compiler if not specified
if ($Compiler -eq 'auto') {
    if (Get-Command clang++ -ErrorAction SilentlyContinue) {
        $Compiler = 'clang'
    } elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        $Compiler = 'cl'
    } elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
        $Compiler = 'gcc'
    } else {
        Write-Error "No suitable C++ compiler found (clang++, cl, or g++ required)."
        return
    }
}

Write-Host "Building $ToolName with $Compiler..." -ForegroundColor Cyan
$OutputFile = "$ToolName.exe"

switch ($Compiler) {
    'clang' {
        & clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN "-Wl,/subsystem:console" -o $OutputFile @SourceFiles
    }
    'cl' {
        & cl /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN "/Fe:$OutputFile" @SourceFiles
    }
    'gcc' {
        & g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -municode -o $OutputFile @SourceFiles
    }
}

if (-not (Test-Path $OutputFile)) {
    Write-Error "Build failed: $OutputFile not found."
    return
}

Write-Host "Build succeeded: $OutputFile" -ForegroundColor Green

if (-not $NoInstall) {
    if ($BinDir -and (Test-Path $BinDir)) {
        $Dest = Join-Path $BinDir "$ToolName.exe"
        Copy-Item -Path $OutputFile -Destination $Dest -Force
        Write-Host "Installed to $Dest" -ForegroundColor Green
    } else {
        Write-Warning "Target bin directory not found. Binary remains in source directory."
    }
}
