<#
.SYNOPSIS
    Builds the bdf utility.

.DESCRIPTION
    Compiles all modular C++ source files for bdf using available compilers (clang++, cl, or g++).
    Optionally installs the resulting binary into the workspace bin/ directory.

.PARAMETER Compiler
    Explicit compiler choice: 'auto', 'clang++', 'cl', or 'g++'. Default is 'auto'.

.PARAMETER Install
    If specified, copies the compiled bdf.exe to the workspace bin/ directory. Default is true.

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
        Write-Host "[bdf] Cleaning build artifacts..." -ForegroundColor Cyan
        Remove-Item -Path "bdf.exe", "*.obj", "*.o", "*.pdb", "*.ilk" -Force -ErrorAction SilentlyContinue
        Write-Host "[bdf] Clean complete." -ForegroundColor Green
        return
    }

    $sources = @(
        "bdf.cpp",
        "bdf_app.cpp",
        "formatter.cpp",
        "inspector.cpp",
        "options.cpp",
        "pipeline.cpp",
        "table.cpp"
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
            Write-Error "[bdf] No supported C++ compiler (clang++, cl.exe, g++) found in PATH."
            exit 1
        }
    }

    Write-Host "[bdf] Compiling with $selectedCompiler..." -ForegroundColor Cyan
    $binOut = "bdf.exe"

    if ($selectedCompiler -eq 'clang++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-o", $binOut)
        & clang++ $cmdArgs
    } elseif ($selectedCompiler -eq 'cl') {
        $cmdArgs = @("/nologo", "/EHsc", "/std:c++17", "/O2") + $sources + @("/Fe:$binOut")
        & cl $cmdArgs
        Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
    } elseif ($selectedCompiler -eq 'g++') {
        $cmdArgs = @("-std=c++17", "-O2", "-municode") + $sources + @("-o", $binOut)
        & g++ $cmdArgs
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "[bdf] Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    Write-Host "[bdf] Build successful: $binOut" -ForegroundColor Green

    if ($Install) {
        $binDir = Join-Path $scriptDir "..\..\bin"
        if (Test-Path $binDir) {
            Copy-Item -Path $binOut -Destination (Join-Path $binDir "bdf.exe") -Force
            Write-Host "[bdf] Installed binary to $(Join-Path $binDir 'bdf.exe')" -ForegroundColor Green
        }
    }
} finally {
    Pop-Location
}
