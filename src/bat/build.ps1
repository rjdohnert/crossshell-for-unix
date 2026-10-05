<#
.SYNOPSIS
    Builds the bat utility.

.DESCRIPTION
    Compiles all modular C++ source files for bat using available compilers (clang++, cl, or g++).
    Optionally installs the resulting binary into the workspace bin/ directory.

.PARAMETER Compiler
    Explicit compiler choice: 'auto', 'clang++', 'cl', or 'g++'. Default is 'auto'.

.PARAMETER Install
    If specified, copies the compiled bat.exe to the workspace bin/ directory. Default is true.

.PARAMETER Clean
    If specified, removes built binaries and intermediate object files.
#>
[CmdletBinding()]
param (
    [ValidateSet('auto', 'clang++', 'cl', 'g++')]
    [string]$Compiler = 'auto',
    [switch]$Install = $true,
    [switch]$Clean
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Clean) {
        Write-Host "[bat] Cleaning build artifacts..." -ForegroundColor Cyan
        Remove-Item -Path "bat.exe", "*.obj", "*.o", "*.pdb", "*.ilk" -Force -ErrorAction SilentlyContinue
        Write-Host "[bat] Clean complete." -ForegroundColor Green
        return
    }

    $sources = @(
        "bat.cpp",
        "bat_app.cpp",
        "engine.cpp",
        "highlighter.cpp",
        "options.cpp",
        "reporter.cpp"
    )

    $selectedCompiler = $Compiler
    if ($selectedCompiler -eq 'auto') {
        if (Get-Command clang++ -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'clang++'
        } elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'cl'
        } elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'g++'
        } else {
            Write-Error "[bat] No supported C++ compiler (clang++, cl.exe, g++) found in PATH."
            exit 1
        }
    }

    Write-Host "[bat] Compiling with $selectedCompiler..." -ForegroundColor Cyan
    $binOut = "bat.exe"

    if ($selectedCompiler -eq 'clang++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-o", $binOut)
        & clang++ $cmdArgs
    } elseif ($selectedCompiler -eq 'cl') {
        $cmdArgs = @("/nologo", "/EHsc", "/std:c++17", "/O2") + $sources + @("/Fe:$binOut")
        & cl $cmdArgs
        Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
    } elseif ($selectedCompiler -eq 'g++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-o", $binOut)
        & g++ $cmdArgs
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "[bat] Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    Write-Host "[bat] Build successful: $binOut" -ForegroundColor Green

    if ($Install) {
        $binDir = Join-Path $scriptDir "..\..\bin"
        if (Test-Path $binDir) {
            Copy-Item -Path $binOut -Destination (Join-Path $binDir "bat.exe") -Force
            Write-Host "[bat] Installed binary to $(Join-Path $binDir 'bat.exe')" -ForegroundColor Green
        }
    }
} finally {
    Pop-Location
}
