<#
.SYNOPSIS
    Builds the sysctl utility.
.DESCRIPTION
    Compiles all modular C++ source files for sysctl using available compilers (clang++, cl, or g++).
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
        Write-Host "[sysctl] Cleaning build artifacts..." -ForegroundColor Cyan
        Remove-Item -Path "sysctl.exe", "*.obj", "*.o", "*.pdb", "*.ilk", "sysctl-tests.exe" -Force -ErrorAction SilentlyContinue
        Write-Host "[sysctl] Clean complete." -ForegroundColor Green
        return
    }

    $sources = @(
        "sysctl.cpp",
        "sysctl_app.cpp",
        "telemetry_provider.cpp",
        "storage_provider.cpp",
        "reporter.cpp",
        "options.cpp",
        "mib_tree.cpp",
        "common.cpp"
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
            Write-Error "[sysctl] No supported C++ compiler (clang++, cl.exe, g++) found in PATH."
            exit 1
        }
    }

    Write-Host "[sysctl] Compiling with $selectedCompiler..." -ForegroundColor Cyan

    $outputExe = Join-Path $scriptDir "sysctl.exe"
    $libs = @("-liphlpapi", "-ladvapi32", "-lsetupapi", "-luuid", "-lpsapi")
    $msvcLibs = @("iphlpapi.lib", "advapi32.lib", "setupapi.lib", "uuid.lib", "psapi.lib")

    if ($selectedCompiler -eq 'clang++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + $libs + @("-Wl,/subsystem:console", "-o", $outputExe)
        & clang++ $cmdArgs
    } elseif ($selectedCompiler -eq 'cl') {
        $cmdArgs = @("/nologo", "/EHsc", "/std:c++17", "/O2") + $sources + $msvcLibs + @("/Fe:$outputExe")
        & cl $cmdArgs
        Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
    } elseif ($selectedCompiler -eq 'g++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + $libs + @("-o", $outputExe)
        & g++ $cmdArgs
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "[sysctl] Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    Write-Host "[sysctl] Successfully built $outputExe" -ForegroundColor Green

    if ($Install) {
        $binDir = Join-Path (Split-Path -Parent $scriptDir) "..\bin"
        $binDir = [System.IO.Path]::GetFullPath($binDir)
        if (Test-Path $binDir) {
            $target = Join-Path $binDir "sysctl.exe"
            Copy-Item -Path $outputExe -Destination $target -Force
            Write-Host "[sysctl] Installed to $target" -ForegroundColor Green
        }
    }
} finally {
    Pop-Location
}
