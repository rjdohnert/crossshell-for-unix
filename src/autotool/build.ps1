<#
.SYNOPSIS
    Builds the autotool utility.

.DESCRIPTION
    Compiles all modular C++ source files for autotool using available compilers (clang++, cl, or g++).
    Optionally installs the resulting binary into the root bin/ directory.

.PARAMETER Compiler
    Explicit compiler choice: 'auto', 'clang++', 'cl', or 'g++'. Default is 'auto'.

.PARAMETER Install
    If specified, copies the compiled autotool.exe to the workspace bin/ directory. Default is true.

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
        Write-Host "[autotool] Cleaning build artifacts..." -ForegroundColor Cyan
        Remove-Item -Path "autotool.exe", "*.obj", "*.o", "*.pdb", "*.ilk" -Force -ErrorAction SilentlyContinue
        Write-Host "[autotool] Clean complete." -ForegroundColor Green
        return
    }

    $sources = @(
        "autoconf.cpp",
        "automake.cpp",
        "autotool.cpp",
        "autotool_app.cpp",
        "build_engine.cpp",
        "help.cpp",
        "libtool.cpp",
        "port_engine.cpp",
        "utils.cpp"
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
            Write-Error "[autotool] No supported C++ compiler (clang++, cl.exe, g++) found in PATH."
            exit 1
        }
    }

    Write-Host "[autotool] Compiling with $selectedCompiler..." -ForegroundColor Cyan

    $outputExe = Join-Path $scriptDir "autotool.exe"

    if ($selectedCompiler -eq 'clang++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-o", $outputExe)
        & clang++ $cmdArgs
    } elseif ($selectedCompiler -eq 'cl') {
        $cmdArgs = @("/nologo", "/EHsc", "/std:c++17", "/O2") + $sources + @("/Fe:$outputExe")
        & cl $cmdArgs
        Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
    } elseif ($selectedCompiler -eq 'g++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-o", $outputExe)
        & g++ $cmdArgs
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "[autotool] Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    Write-Host "[autotool] Successfully built $outputExe" -ForegroundColor Green

    if ($Install) {
        $binDir = Resolve-Path (Join-Path $scriptDir "..\..\bin") -ErrorAction SilentlyContinue
        if ($binDir -and (Test-Path $binDir)) {
            $target = Join-Path $binDir "autotool.exe"
            Copy-Item -Path $outputExe -Destination $target -Force
            Write-Host "[autotool] Installed to $target" -ForegroundColor Green
        }
    }
}
finally {
    Pop-Location
}
