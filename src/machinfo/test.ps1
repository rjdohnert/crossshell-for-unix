[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang++', 'cl', 'g++')][string]$Compiler = 'auto'
)

$ErrorActionPreference = 'Stop'
$scriptDir = $PSScriptRoot
$build = Join-Path $scriptDir 'build.ps1'
& $build -Compiler $Compiler -Install:$false
if ($LASTEXITCODE -ne 0) { throw 'machinfo build failed.' }

$selectedCompiler = $Compiler
if ($selectedCompiler -eq 'auto') {
    if (Get-Command clang++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'clang++' }
    elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $selectedCompiler = 'cl' }
    else { $selectedCompiler = 'g++' }
}

$testRoot = Join-Path $env:TEMP ('machinfo-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$unitTest = Join-Path $testRoot 'machinfo-unit-tests.exe'

function Assert-True([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}

function Invoke-Machinfo([string[]]$arguments) {
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = (Join-Path $scriptDir 'machinfo.exe')
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [System.Text.UnicodeEncoding]::new($false, $false)
    $info.StandardErrorEncoding = [System.Text.UnicodeEncoding]::new($false, $false)
    foreach ($argument in $arguments) { $info.ArgumentList.Add($argument) }
    $process = [System.Diagnostics.Process]::Start($info)
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    return [pscustomobject]@{ ExitCode = $process.ExitCode; Output = $stdout; Error = $stderr }
}

try {
    Push-Location $testRoot
    try {
        if ($selectedCompiler -eq 'cl') {
            $unitSources = @(
                (Join-Path $scriptDir 'tests.cpp'),
                (Join-Path $scriptDir 'reporter.cpp'),
                (Join-Path $scriptDir 'options.cpp'),
                (Join-Path $scriptDir 'engine.cpp')
            )
            & cl.exe /nologo /EHsc /std:c++17 /O2 "/Fo$testRoot\" "/Fe:$unitTest" @unitSources `
                wbemuuid.lib ole32.lib oleaut32.lib tbs.lib advapi32.lib
        } elseif ($selectedCompiler -eq 'g++') {
            & g++ -std=c++17 -O2 (Join-Path $scriptDir 'tests.cpp') `
                (Join-Path $scriptDir 'reporter.cpp') (Join-Path $scriptDir 'options.cpp') `
                (Join-Path $scriptDir 'engine.cpp') -lwbemuuid -lole32 -loleaut32 -ltbs -ladvapi32 -o $unitTest
        } else {
            & clang++ -std=c++17 -O2 (Join-Path $scriptDir 'tests.cpp') `
                (Join-Path $scriptDir 'reporter.cpp') (Join-Path $scriptDir 'options.cpp') `
                (Join-Path $scriptDir 'engine.cpp') -lwbemuuid -lole32 -loleaut32 -ltbs -ladvapi32 -o $unitTest
        }
        if ($LASTEXITCODE -ne 0) { throw 'machinfo unit-test build failed.' }
        & $unitTest
        if ($LASTEXITCODE -ne 0) { throw "machinfo unit tests failed ($LASTEXITCODE)." }
    } finally {
        Pop-Location
    }

    $jsonRun = Invoke-Machinfo @('--json')
    Assert-True ($jsonRun.ExitCode -eq 0) "JSON command failed: $($jsonRun.Error)"
    $json = $jsonRun.Output | ConvertFrom-Json
    foreach ($field in @('manufacturer', 'cpu_model', 'cpu_architecture',
                         'cpu_logical_processors', 'memory_total_physical_bytes',
                         'tpm_present', 'tpm_ready', 'os_version')) {
        Assert-True ($null -ne $json.PSObject.Properties[$field]) "JSON field missing: $field"
    }

    $csvRun = Invoke-Machinfo @('--format=csv')
    Assert-True ($csvRun.ExitCode -eq 0) "CSV command failed: $($csvRun.Error)"
    $csvRows = @($csvRun.Output | ConvertFrom-Csv)
    Assert-True ($csvRows.Count -gt 0 -and $csvRows[0].PSObject.Properties.Name -contains 'Field' -and
                 $csvRows[0].PSObject.Properties.Name -contains 'Value') 'CSV output did not parse as Field,Value records.'
    Assert-True (@($csvRows.Field) -contains 'cpu_architecture') 'CSV omitted the CPU architecture field.'

    $filteredRun = Invoke-Machinfo @('--json', 'cpu')
    Assert-True ($filteredRun.ExitCode -eq 0) "Filtered JSON command failed: $($filteredRun.Error)"
    $filtered = $filteredRun.Output | ConvertFrom-Json
    Assert-True ($null -ne $filtered.PSObject.Properties['cpu_model'] -and
                 $null -eq $filtered.PSObject.Properties['memory_total_physical_bytes']) 'CPU section filter did not limit output.'

    $badFormat = Invoke-Machinfo @('--format=xml')
    Assert-True ($badFormat.ExitCode -ne 0) 'Unsupported format should fail.'
    $missingFormat = Invoke-Machinfo @('--format')
    Assert-True ($missingFormat.ExitCode -ne 0) 'Missing format value should fail.'
    $badSection = Invoke-Machinfo @('--json', 'not-a-section')
    Assert-True ($badSection.ExitCode -ne 0) 'Unknown section filter should fail.'

    'PASS: escaping, option parsing, SMBIOS fallbacks, JSON/CSV parsing, section filters, and invalid arguments.'
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}