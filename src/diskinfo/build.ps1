<#
.SYNOPSIS
    Builds the diskinfo utility.
.DESCRIPTION
    Compiles all modular C++ source files for diskinfo using available compilers (clang++, cl, or g++).
    Optionally installs the resulting binary into the root bin/ directory.
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
        Write-Host "[diskinfo] Cleaning build artifacts..." -ForegroundColor Cyan
        Remove-Item -Path "diskinfo.exe", "*.obj", "*.o", "*.pdb", "*.ilk" -Force -ErrorAction SilentlyContinue
        Write-Host "[diskinfo] Clean complete." -ForegroundColor Green
        return
    }

    $sources = @(
        "diskinfo.cpp",
        "diskinfo_app.cpp",
        "engine.cpp",
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
            Write-Error "[diskinfo] No supported C++ compiler (clang++, cl.exe, g++) found in PATH."
            exit 1
        }
    }

    Write-Host "[diskinfo] Compiling with $selectedCompiler..." -ForegroundColor Cyan

    $outputExe = Join-Path $scriptDir "diskinfo.exe"

    if ($selectedCompiler -eq 'clang++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-lmpr", "-Wl,/subsystem:console", "-o", $outputExe)
        & clang++ $cmdArgs
    } elseif ($selectedCompiler -eq 'cl') {
        $cmdArgs = @("/nologo", "/EHsc", "/std:c++17", "/O2") + $sources + @("mpr.lib", "/Fe:$outputExe")
        & cl $cmdArgs
        Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
    } elseif ($selectedCompiler -eq 'g++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-lmpr", "-o", $outputExe)
        & g++ $cmdArgs
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "[diskinfo] Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    Write-Host "[diskinfo] Successfully built $outputExe" -ForegroundColor Green

    if ($Install) {
        $binDir = Resolve-Path (Join-Path $scriptDir "..\..\bin") -ErrorAction SilentlyContinue
        if ($binDir -and (Test-Path $binDir)) {
            $target = Join-Path $binDir "diskinfo.exe"
            Copy-Item -Path $outputExe -Destination $target -Force
            Write-Host "[diskinfo] Installed to $target" -ForegroundColor Green
        }
    }
}
finally {
    Pop-Location
}
