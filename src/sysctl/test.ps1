[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang++', 'cl', 'g++')][string]$Compiler = 'auto'
)

$ErrorActionPreference = 'Stop'
$scriptDir = $PSScriptRoot
$build = Join-Path $scriptDir 'build.ps1'
& $build -Compiler $Compiler -Install:$false
if ($LASTEXITCODE -ne 0) { throw 'sysctl build failed.' }

$selectedCompiler = $Compiler
if ($selectedCompiler -eq 'auto') {
    if (Get-Command clang++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'clang++' }
    elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $selectedCompiler = 'cl' }
    else { $selectedCompiler = 'g++' }
}

$testRoot = Join-Path $env:TEMP ('sysctl-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$unitTest = Join-Path $testRoot 'sysctl-unit-tests.exe'

function Assert-True([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}

function Invoke-Sysctl([string[]]$arguments) {
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = (Join-Path $scriptDir 'sysctl.exe')
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $info.StandardErrorEncoding = [System.Text.UTF8Encoding]::new($false)
    if ($null -ne $info.PSObject.Properties['ArgumentList'] -and $null -ne $info.ArgumentList) {
        foreach ($argument in $arguments) { [void]$info.ArgumentList.Add($argument) }
    } else {
        $info.Arguments = ($arguments -join ' ')
    }
    $process = [System.Diagnostics.Process]::Start($info)
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    return [pscustomobject]@{ ExitCode = $process.ExitCode; Output = $stdout; Error = $stderr }
}

try {
    Push-Location $testRoot
    try {
        $unitSources = @(
            (Join-Path $scriptDir 'tests.cpp'),
            (Join-Path $scriptDir 'sysctl_app.cpp'),
            (Join-Path $scriptDir 'telemetry_provider.cpp'),
            (Join-Path $scriptDir 'storage_provider.cpp'),
            (Join-Path $scriptDir 'reporter.cpp'),
            (Join-Path $scriptDir 'options.cpp'),
            (Join-Path $scriptDir 'mib_tree.cpp'),
            (Join-Path $scriptDir 'common.cpp')
        )
        $libs = @("-liphlpapi", "-ladvapi32", "-lsetupapi", "-luuid", "-lpsapi")
        $msvcLibs = @("iphlpapi.lib", "advapi32.lib", "setupapi.lib", "uuid.lib", "psapi.lib")

        if ($selectedCompiler -eq 'cl') {
            & cl.exe /nologo /EHsc /std:c++17 /O2 "/Fo$testRoot\" "/Fe:$unitTest" $unitSources $msvcLibs
        } elseif ($selectedCompiler -eq 'g++') {
            & g++ -std=c++17 -O2 $unitSources $libs -o $unitTest
        } else {
            & clang++ -std=c++17 -O2 $unitSources $libs -o $unitTest
        }
        if ($LASTEXITCODE -ne 0) { throw 'sysctl unit-test build failed.' }
        & $unitTest
        if ($LASTEXITCODE -ne 0) { throw "sysctl unit tests failed ($LASTEXITCODE)." }
    } finally {
        Pop-Location
    }

    $runA = Invoke-Sysctl @('-a')
    Assert-True ($runA.ExitCode -eq 0) 'sysctl -a failed.'
    Assert-True ($runA.Output.Contains('kern.ostype=Windows_NT')) 'kern.ostype missing from sysctl -a output.'

    $runN = Invoke-Sysctl @('-n', 'kern.ostype')
    Assert-True ($runN.ExitCode -eq 0 -and $runN.Output.Trim() -eq 'Windows_NT') 'sysctl -n kern.ostype failed.'

    $runNames = Invoke-Sysctl @('-N', 'kern')
    Assert-True ($runNames.ExitCode -eq 0 -and ($runNames.Output -match 'kern\.ostype')) 'sysctl -N kern failed.'

    $runWildcard = Invoke-Sysctl @('hw.*')
    Assert-True ($runWildcard.ExitCode -eq 0 -and $runWildcard.Output.Contains('hw.machine=')) 'sysctl hw.* failed.'

    'PASS: modular unit tests, sysctl CLI execution, MIB traversal, -n, -N, and wildcard lookups.'
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}
