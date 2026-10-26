<#
.SYNOPSIS
    Builds the netconf utility.

.DESCRIPTION
    Compiles netconf using available compilers (clang, clang++, cl, gcc, or g++).
    Optionally installs the resulting binary into the workspace bin/ directory.

.PARAMETER Compiler
    Explicit compiler choice: 'auto', 'clang', 'clang++', 'cl', 'gcc', or 'g++'. Default is 'auto'.

.PARAMETER Install
    If specified, copies the compiled netconf.exe to the workspace bin/ directory. Default is true.

.PARAMETER Clean
    If specified, removes built binaries and intermediate object files.
#>
[CmdletBinding()]
param (
    [ValidateSet('auto', 'clang', 'clang++', 'cl', 'gcc', 'g++')]
    [string]$Compiler = 'auto',
    [switch]$Install = $true,
    [switch]$Clean
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if ($Clean) {
        Write-Host "[netconf] Cleaning build artifacts..." -ForegroundColor Cyan
        Remove-Item -Path "netconf.exe", "*.obj", "*.o", "*.pdb", "*.ilk" -Force -ErrorAction SilentlyContinue
        Write-Host "[netconf] Clean complete." -ForegroundColor Green
        return
    }

    $sources = @("netconf.c")
    $libs = @("-liphlpapi", "-lws2_32")
    $msvcLibs = @("iphlpapi.lib", "ws2_32.lib")

    $selectedCompiler = $Compiler
    if ($selectedCompiler -eq 'auto') {
        if (Get-Command clang -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'clang'
        } elseif (Get-Command clang++ -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'clang++'
        } elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'cl'
        } elseif (Get-Command gcc -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'gcc'
        } elseif (Get-Command g++ -ErrorAction SilentlyContinue) {
            $selectedCompiler = 'g++'
        } else {
            Write-Error "[netconf] No supported C/C++ compiler (clang, clang++, cl.exe, gcc, g++) found in PATH."
            exit 1
        }
    }

    Write-Host "[netconf] Compiling with $selectedCompiler..." -ForegroundColor Cyan

    $outputExe = Join-Path $scriptDir "netconf.exe"

    if ($selectedCompiler -in @('clang', 'clang++', 'gcc', 'g++')) {
        $cmdArgs = @("-O2") + $sources + $libs + @("-o", $outputExe)
        & $selectedCompiler $cmdArgs
    } elseif ($selectedCompiler -eq 'cl') {
        $cmdArgs = @("/nologo", "/O2") + $sources + $msvcLibs + @("/Fe:$outputExe")
        & cl $cmdArgs
        Remove-Item -Path "*.obj" -Force -ErrorAction SilentlyContinue
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "[netconf] Compilation failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }

    Write-Host "[netconf] Successfully built $outputExe" -ForegroundColor Green

    if ($Install) {
        $binDir = Resolve-Path (Join-Path $scriptDir "..\..\bin") -ErrorAction SilentlyContinue
        if ($binDir -and (Test-Path $binDir)) {
            $target = Join-Path $binDir "netconf.exe"
            Copy-Item -Path $outputExe -Destination $target -Force
            Write-Host "[netconf] Installed to $target" -ForegroundColor Green
        }
    }
}
finally {
    Pop-Location
}
