<#
.SYNOPSIS
    Builds the a2pdf utility.

.DESCRIPTION
    Compiles all modular C++ source files for a2pdf using available compilers (clang++, cl, or g++).
    Optionally installs the resulting binary into the root bin/ directory.

.PARAMETER Compiler
    Explicit compiler choice: 'auto', 'clang++', 'cl', or 'g++'. Default is 'auto'.

.PARAMETER Install
    If specified, copies the compiled a2pdf.exe to the workspace bin/ directory. Default is true.

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
        Write-Host "[a2pdf] Cleaning build artifacts..." -ForegroundColor Cyan
        Remove-Item -Path "a2pdf.exe", "*.obj", "*.o", "*.pdb", "*.ilk" -Force -ErrorAction SilentlyContinue
        Write-Host "[a2pdf] Clean complete." -ForegroundColor Green
        return
    }

    $sources = @(
        "a2pdf.cpp",
        "a2pdf_app.cpp",
        "cli.cpp",
        "encoding.cpp",
        "output_reporter.cpp",
        "pdf_generator.cpp",
        "ps_generator.cpp",
        "text_processor.cpp"
    )

    # Determine compiler
    $selectedCompiler = $Compiler
    if ($selectedCompiler -eq 'auto') {
        if (Get-Command clang++ -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'clang++'
        } elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'cl'
        } elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'g++'
        } else {
            Write-Error "[a2pdf] No supported C++ compiler (clang++, cl.exe, g++) found in PATH."
            exit 1
        }
    }

    Write-Host "[a2pdf] Compiling with $selectedCompiler..." -ForegroundColor Cyan

    $outputExe = Join-Path $scriptDir "a2pdf.exe"

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
        Write-Error "[a2pdf] Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    Write-Host "[a2pdf] Successfully built $outputExe" -ForegroundColor Green

    if ($Install) {
        $binDir = Resolve-Path (Join-Path $scriptDir "..\..\bin") -ErrorAction SilentlyContinue
        if ($binDir -and (Test-Path $binDir)) {
            $target = Join-Path $binDir "a2pdf.exe"
            Copy-Item -Path $outputExe -Destination $target -Force
            Write-Host "[a2pdf] Installed to $target" -ForegroundColor Green
        }
    }
}
finally {
    Pop-Location
}
