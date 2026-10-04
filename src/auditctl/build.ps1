<#
.SYNOPSIS
    Builds the auditctl utility.

.DESCRIPTION
    Compiles all modular C++ source files for auditctl using available compilers (clang++, cl, or g++).
    Optionally installs the resulting binary into the root bin/ directory.

.PARAMETER Compiler
    Explicit compiler choice: 'auto', 'clang++', 'cl', or 'g++'. Default is 'auto'.

.PARAMETER Install
    If specified, copies the compiled auditctl.exe to the workspace bin/ directory. Default is true.

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
        Write-Host "[auditctl] Cleaning build artifacts..." -ForegroundColor Cyan
        Remove-Item -Path "auditctl.exe", "*.obj", "*.o", "*.pdb", "*.ilk" -Force -ErrorAction SilentlyContinue
        Write-Host "[auditctl] Clean complete." -ForegroundColor Green
        return
    }

    $sources = @(
        "auditctl.cpp",
        "auditctl_app.cpp",
        "audit_policy.cpp",
        "audit_session.cpp",
        "audit_types.cpp",
        "cli.cpp"
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
            Write-Error "[auditctl] No supported C++ compiler (clang++, cl.exe, g++) found in PATH."
            exit 1
        }
    }

    Write-Host "[auditctl] Compiling with $selectedCompiler..." -ForegroundColor Cyan

    $outputExe = Join-Path $scriptDir "auditctl.exe"

    if ($selectedCompiler -eq 'clang++') {
        $cmdArgs = @("-std=c++17", "-O2") + $sources + @("-ladvapi32", "-o", $outputExe)
        & clang++ $cmdArgs
    } elseif ($selectedCompiler -eq 'cl') {
        $cmdArgs = @("/nologo", "/EHsc", "/std:c++17", "/O2") + $sources + @("Advapi32.lib", "/Fe:$outputExe")
        & cl $cmdArgs
        Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
    } elseif ($selectedCompiler -eq 'g++') {
        $cmdArgs = @("-std=c++17", "-O2", "-municode") + $sources + @("-ladvapi32", "-o", $outputExe)
        & g++ $cmdArgs
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "[auditctl] Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    Write-Host "[auditctl] Successfully built $outputExe" -ForegroundColor Green

    if ($Install) {
        $binDir = Resolve-Path (Join-Path $scriptDir "..\..\bin") -ErrorAction SilentlyContinue
        if ($binDir -and (Test-Path $binDir)) {
            $target = Join-Path $binDir "auditctl.exe"
            Copy-Item -Path $outputExe -Destination $target -Force
            Write-Host "[auditctl] Installed to $target" -ForegroundColor Green
        }
    }
}
finally {
    Pop-Location
}
