[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'clang++', 'cl', 'gcc', 'g++')][string]$Compiler = 'auto'
)

$ErrorActionPreference = 'Stop'
$scriptDir = $PSScriptRoot
$build = Join-Path $scriptDir 'build.ps1'

& $build -Compiler $Compiler -Install:$false
if ($LASTEXITCODE -ne 0) { throw 'netconf build failed.' }

$netconfExe = Join-Path $scriptDir 'netconf.exe'

function Assert-True([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}

function Invoke-Netconf([string[]]$arguments) {
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $netconfExe
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

Write-Host "[netconf-test] Testing version output..." -ForegroundColor Cyan
$runVer = Invoke-Netconf @('-v')
Assert-True ($runVer.ExitCode -eq 0) 'netconf -v failed'
Assert-True ($runVer.Output -match 'netconf version 2\.3\.0') 'Version string mismatch'

Write-Host "[netconf-test] Testing help manual..." -ForegroundColor Cyan
$runHelp = Invoke-Netconf @('--help')
Assert-True ($runHelp.ExitCode -eq 0) 'netconf --help failed'
Assert-True ($runHelp.Output -match 'SYNOPSIS' -and $runHelp.Output -match 'OPTIONS') 'Help text missing expected sections'

Write-Host "[netconf-test] Testing default table output..." -ForegroundColor Cyan
$runDefault = Invoke-Netconf @()
Assert-True ($runDefault.ExitCode -eq 0) 'Default execution failed'
Assert-True ($runDefault.Output.Contains('PORT') -and $runDefault.Output.Contains('STATION_ADDR')) 'Table headers missing'

Write-Host "[netconf-test] Testing detail output (-d)..." -ForegroundColor Cyan
$runDetail = Invoke-Netconf @('-d')
Assert-True ($runDetail.ExitCode -eq 0) 'netconf -d failed'
Assert-True ($runDetail.Output.Contains('IPv4:') -and $runDetail.Output.Contains('MTU:')) 'Detail fields missing'

Write-Host "[netconf-test] Testing JSON output (-j)..." -ForegroundColor Cyan
$runJson = Invoke-Netconf @('-j')
Assert-True ($runJson.ExitCode -eq 0) 'netconf -j failed'
$parsed = $runJson.Output | ConvertFrom-Json
Assert-True ($parsed.Count -gt 0) 'JSON output produced 0 records'
Assert-True ($null -ne $parsed[0].port -and $null -ne $parsed[0].station_address) 'JSON record structure missing required fields'

Write-Host "[netconf-test] Testing CSV output (-c)..." -ForegroundColor Cyan
$runCsv = Invoke-Netconf @('-c')
Assert-True ($runCsv.ExitCode -eq 0) 'netconf -c failed'
Assert-True ($runCsv.Output -match '^port,type,link_state') 'CSV header mismatch'

Write-Host "[netconf-test] Testing port filtering (lan0)..." -ForegroundColor Cyan
$runFilter = Invoke-Netconf @('lan0')
Assert-True ($runFilter.ExitCode -eq 0) 'netconf lan0 failed'
Assert-True ($runFilter.Output.Contains('lan0')) 'Target port lan0 not found in output'

Write-Host "[netconf-test] Testing unknown interface error..." -ForegroundColor Cyan
$runMissing = Invoke-Netconf @('nonexistent_dummy_9999')
Assert-True ($runMissing.ExitCode -ne 0) 'netconf should return non-zero for nonexistent interface'
Assert-True ($runMissing.Error.Contains('no network interfaces matched')) 'Expected error message missing'

Write-Host "[netconf-test] Testing invalid flag handling..." -ForegroundColor Cyan
$runBad = Invoke-Netconf @('--invalid-flag-xyz')
Assert-True ($runBad.ExitCode -ne 0) 'netconf should return non-zero for invalid option'
Assert-True ($runBad.Error.Contains('unrecognized option')) 'Expected unrecognized option error'

Write-Host "PASS: All netconf test suites passed successfully." -ForegroundColor Green
