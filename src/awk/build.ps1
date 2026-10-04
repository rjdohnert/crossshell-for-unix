<#
.SYNOPSIS
    Builds the awk utility.

.DESCRIPTION
    Compiles all modular C++ source files for awk using available compilers (clang++, cl, or g++).
    Optionally installs the resulting binary into the workspace bin/ directory.

.PARAMETER Compiler
    Explicit compiler choice: 'auto', 'clang++', 'cl', or 'g++'. Default is 'auto'.

.PARAMETER Install
    If specified, copies the compiled awk.exe to the workspace bin/ directory. Default is true.

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
        Write-Host "[awk] Cleaning build artifacts..." -ForegroundColor Cyan
        Remove-Item -Path "awk.exe", "*.obj", "*.o", "*.pdb", "*.ilk" -Force -ErrorAction SilentlyContinue
        Write-Host "[awk] Clean complete." -ForegroundColor Green
        return
    }

    $sources = @(
        "awk.cpp",
        "awk_app.cpp",
        "context.cpp",
        "evaluator.cpp",
        "parser.cpp",
        "string_utils.cpp",
        "value.cpp",
        "win_loader.cpp"
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
            Write-Error "[awk] No supported C++ compiler (clang++, cl.exe, g++) found in PATH."
            exit 1
        }
    }

    Write-Host "[awk] Compiling with $selectedCompiler..." -ForegroundColor Cyan
    $binOut = "awk.exe"

    if ($selectedCompiler -eq 'clang++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-lwbemuuid", "-lole32", "-loleaut32", "-ladvapi32", "-o", $binOut)
        & clang++ $cmdArgs
    } elseif ($selectedCompiler -eq 'cl') {
        $cmdArgs = @("/nologo", "/EHsc", "/std:c++17", "/O2") + $sources + @("wbemuuid.lib", "ole32.lib", "oleaut32.lib", "advapi32.lib", "/Fe:$binOut")
        & cl $cmdArgs
        Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
    } elseif ($selectedCompiler -eq 'g++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-lwbemuuid", "-lole32", "-loleaut32", "-ladvapi32", "-o", $binOut)
        & g++ $cmdArgs
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "[awk] Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    Write-Host "[awk] Build successful: $binOut" -ForegroundColor Green

    if ($Install) {
        $binDir = Join-Path $scriptDir "..\..\bin"
        if (Test-Path $binDir) {
            Copy-Item -Path $binOut -Destination (Join-Path $binDir "awk.exe") -Force
            Write-Host "[awk] Installed binary to $(Join-Path $binDir 'awk.exe')" -ForegroundColor Green
        }
    }
} finally {
    Pop-Location
}
