<#
.SYNOPSIS
    CrossShell KSH Build Script (PowerShell)
.DESCRIPTION
    Builds CrossShell KSH either as a single-file SQLite-style amalgamation
    (ksh.cpp) or from separate modular translation units (src/*.cpp).
    Installs by default to bin/ksh.exe and supports running the internal self-test suite.
.PARAMETER Action
    Action to perform: 'build' (default), 'clean', 'test', 'noinstall', 'modular', 'amalgamate'.
.PARAMETER Target
    Build target: 'amalgamation' (default), 'modular', or 'all'.
.PARAMETER Compiler
    Compiler to use: auto-detected by default (clang++, then cl).
.EXAMPLE
    .\build.ps1
    .\build.ps1 clean
    .\build.ps1 test
    .\build.ps1 modular
    .\build.ps1 amalgamate
    .\build.ps1 -Compiler cl
#>

param(
    [string]$Action = "build",
    [string]$Target = "amalgamation",
    [string]$Compiler = ""
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    # Normalize positional argument if passed
    if ($Action -match "^(clean|test|noinstall|modular|amalgamate|all)$") {
        if ($Action -eq "modular") {
            $Target = "modular"
            $Action = "build"
        } elseif ($Action -eq "amalgamate") {
            $Action = "amalgamate"
        }
    }

    if ($Action -eq "clean") {
        Remove-Item -Force -ErrorAction SilentlyContinue ksh.exe, ksh_modular.exe, amalgamate.exe, *.obj, *.o, *.pdb, *.ilk
        Write-Host "[ksh] Clean complete."
        return
    }

    if ($Action -eq "amalgamate") {
        Write-Host "[ksh] Generating amalgamation..."
        if (Test-Path "$scriptDir\amalgamate.exe") {
            & "$scriptDir\amalgamate.exe" --source-dir "$scriptDir" --output "$scriptDir\ksh.cpp"
        } else {
            & pwsh.exe -NoProfile -File "$scriptDir\amalgamate.ps1" -OutputFile "$scriptDir\ksh.cpp" -SourceDir "$scriptDir"
        }
        Write-Host "[ksh] Amalgamation updated: ksh.cpp"
        return
    }

    # Locate compiler
    $compilerCmd = ""
    if ($Compiler -ne "") {
        $compilerCmd = $Compiler
    } elseif (Get-Command clang++ -ErrorAction SilentlyContinue) {
        $compilerCmd = "clang++"
    } elseif (Test-Path "C:\Program Files\LLVM\bin\clang++.exe") {
        $compilerCmd = "C:\Program Files\LLVM\bin\clang++.exe"
    } elseif (Get-Command cl -ErrorAction SilentlyContinue) {
        $compilerCmd = "cl"
    } else {
        Write-Error "[ksh] Error: Neither clang++ nor cl.exe found in PATH."
        exit 1
    }

    Write-Host "[ksh] Using compiler: $compilerCmd"

    # Step 1: Ensure amalgamation exists
    if ($Target -eq "amalgamation" -or $Target -eq "all") {
        if (-not (Test-Path "$scriptDir\ksh.cpp")) {
            Write-Host "[ksh] Generating initial amalgamation..."
            if (Test-Path "$scriptDir\amalgamate.exe") {
                & "$scriptDir\amalgamate.exe" --source-dir "$scriptDir" --output "$scriptDir\ksh.cpp"
            } else {
                & pwsh.exe -NoProfile -File "$scriptDir\amalgamate.ps1" -OutputFile "$scriptDir\ksh.cpp" -SourceDir "$scriptDir"
            }
        }
    }

    $isMsvc = ($compilerCmd -eq "cl" -or $compilerCmd -like "*\cl.exe")

    # Step 2: Build target
    if ($Target -eq "amalgamation" -or $Target -eq "all") {
        Write-Host "[ksh] Compiling amalgamated ksh.exe..."
        if ($isMsvc) {
            & $compilerCmd /std:c++17 /O2 /EHsc /nologo /Fe:ksh.exe ksh.cpp Advapi32.lib User32.lib
        } else {
            & $compilerCmd -std=c++17 -O2 ksh.cpp -lAdvapi32 -lUser32 "-Wl,/subsystem:console" -o ksh.exe
        }
        if ($LASTEXITCODE -ne 0) {
            Write-Error "[ksh] Build failed for ksh.exe."
            exit $LASTEXITCODE
        }
        Write-Host "[ksh] Build successful: ksh.exe"
    }

    if ($Target -eq "modular" -or $Target -eq "all") {
        Write-Host "[ksh] Compiling modular ksh_modular.exe..."
        $sources = Get-ChildItem -Path "$scriptDir\src\*.cpp" | ForEach-Object { $_.FullName }
        if ($isMsvc) {
            & $compilerCmd /std:c++17 /O2 /EHsc /nologo /I "$scriptDir\include" /Fe:ksh_modular.exe $sources Advapi32.lib User32.lib
        } else {
            & $compilerCmd -std=c++17 -O2 -I "$scriptDir\include" $sources -lAdvapi32 -lUser32 "-Wl,/subsystem:console" -o ksh_modular.exe
        }
        if ($LASTEXITCODE -ne 0) {
            Write-Error "[ksh] Build failed for ksh_modular.exe."
            exit $LASTEXITCODE
        }
        Write-Host "[ksh] Build successful: ksh_modular.exe"
    }

    # Step 3: Run Self-Tests if requested
    if ($Action -eq "test") {
        Write-Host "[ksh] Running internal self-tests..."
        $testTarget = if ($Target -eq "modular") { ".\ksh_modular.exe" } else { ".\ksh.exe" }
        & $testTarget --self-test
        if ($LASTEXITCODE -ne 0) {
            Write-Error "[ksh] Self-tests failed."
            exit $LASTEXITCODE
        }
        Write-Host "[ksh] All self-tests passed!"
    }

    # Step 4: Install to ../../bin/ if requested
    if ($Action -ne "noinstall") {
        $binDir = Join-Path $scriptDir "..\..\bin"
        if (Test-Path $binDir) {
            if (Test-Path "ksh.exe") {
                Copy-Item -Force "ksh.exe" (Join-Path $binDir "ksh.exe")
                Write-Host "[ksh] Installed to ..\..\bin\ksh.exe"
            }
        }
    }
}
finally {
    Pop-Location
}
