[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang++', 'cl', 'g++')][string]$Compiler = 'auto'
)

$ErrorActionPreference = 'Stop'
$scriptDir = $PSScriptRoot
& (Join-Path $scriptDir 'build.ps1') -Compiler $Compiler -Install:$false
if ($LASTEXITCODE -ne 0) { throw 'diskinfo build failed.' }

$selectedCompiler = $Compiler
if ($selectedCompiler -eq 'auto') {
    if (Get-Command clang++ -ErrorAction SilentlyContinue) { $selectedCompiler = 'clang++' }
    elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $selectedCompiler = 'cl' }
    else { $selectedCompiler = 'g++' }
}

$testRoot = Join-Path $env:TEMP ('diskinfo-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot | Out-Null
$testExe = Join-Path $testRoot 'diskinfo-tests.exe'

function Assert-True([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}

function Invoke-DiskinfoUtf8([string[]]$arguments) {
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = (Join-Path $scriptDir 'diskinfo.exe')
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false)
    $info.StandardErrorEncoding = [System.Text.UTF8Encoding]::new($false)
    if ($null -ne $info.PSObject.Properties['ArgumentList'] -and $null -ne $info.ArgumentList) {
        foreach ($argument in $arguments) { $info.ArgumentList.Add($argument) }
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
        if ($selectedCompiler -eq 'cl') {
            & cl.exe /nologo /EHsc /std:c++17 /O2 "/Fo$testRoot\" "/Fe:$testExe" `
                (Join-Path $scriptDir 'tests.cpp') (Join-Path $scriptDir 'engine.cpp') `
                (Join-Path $scriptDir 'reporter.cpp') mpr.lib
        } elseif ($selectedCompiler -eq 'g++') {
            & g++ -std=c++17 -O2 (Join-Path $scriptDir 'tests.cpp') `
                (Join-Path $scriptDir 'engine.cpp') (Join-Path $scriptDir 'reporter.cpp') -lmpr -o $testExe
        } else {
            & clang++ -std=c++17 -O2 (Join-Path $scriptDir 'tests.cpp') `
                (Join-Path $scriptDir 'engine.cpp') (Join-Path $scriptDir 'reporter.cpp') -lmpr -o $testExe
        }
        if ($LASTEXITCODE -ne 0) { throw 'diskinfo unit-test build failed.' }
        & $testExe
        if ($LASTEXITCODE -ne 0) { throw "diskinfo unit tests failed ($LASTEXITCODE)." }
    } finally {
        Pop-Location
    }

    $jsonRun = Invoke-DiskinfoUtf8 @('--json')
    Assert-True ($jsonRun.ExitCode -eq 0 -and $jsonRun.Output.StartsWith('[')) 'JSON output has a failure status, leading blank line, or invalid prefix.'
    $rows = @($jsonRun.Output | ConvertFrom-Json)
    if ($rows.Count -eq 1 -and $rows[0] -is [System.Array]) { $rows = $rows[0] }
    Assert-True ($rows.Count -gt 0) 'No logical volumes were enumerated.'
    foreach ($row in $rows) {
        foreach ($field in @('name', 'type', 'parent', 'fsType', 'mountPoint', 'sizeBytes', 'size', 'readOnly')) {
            Assert-True ($null -ne $row.PSObject.Properties[$field]) "JSON field missing: $field"
        }
        if ($null -ne $row.sizeBytes) {
            Assert-True ($row.sizeBytes -is [long] -or $row.sizeBytes -is [int] -or $row.sizeBytes -is [decimal]) 'sizeBytes must be numeric JSON.'
        }
        if ($null -ne $row.readOnly) {
            Assert-True ($row.readOnly -is [bool]) 'readOnly must be boolean or null.'
        }
    }
    $keys = @($rows | ForEach-Object { "$($_.name)|$($_.type)" })
    Assert-True (($keys | Select-Object -Unique).Count -eq $keys.Count) 'JSON output contains duplicate name/type records.'

    $csvRun = Invoke-DiskinfoUtf8 @('--csv')
    Assert-True ($csvRun.ExitCode -eq 0) 'CSV output failed.'
    $csvRows = @($csvRun.Output | ConvertFrom-Csv)
    Assert-True ($csvRows.Count -gt 0 -and $csvRows[0].PSObject.Properties.Name -contains 'SIZE_BYTES') 'CSV output schema or quoting is invalid.'

    $noHeaderRun = Invoke-DiskinfoUtf8 @('--csv', '-n')
    Assert-True ($noHeaderRun.ExitCode -eq 0 -and $noHeaderRun.Output -notmatch '^NAME,TYPE,') '-n did not suppress CSV headings.'
    $noHeaderRows = @($noHeaderRun.Output | ConvertFrom-Csv -Header NAME,TYPE,PKNAME,FSTYPE,MOUNTPOINT,SIZE_BYTES,SIZE,RO)
    Assert-True ($noHeaderRows.Count -gt 0) 'CSV without headings contained no data records.'

    'PASS: escaping, tree table, topology JSON, CSV parsing, and noheadings.'
} finally {
    Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction SilentlyContinue
}