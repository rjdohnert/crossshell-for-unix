param(
    [Parameter(Mandatory = $true)]
    [string]$CrossShellPath,
    [Parameter(Mandatory = $true)]
    [string]$ReferenceShellPath,
    [string]$ReferenceWslDistribution = ""
)

$ErrorActionPreference = "Stop"
$crossShell = (Resolve-Path -LiteralPath $CrossShellPath).Path
$wslExecutable = $null
if ([string]::IsNullOrWhiteSpace($ReferenceWslDistribution)) {
    $referenceShell = (Resolve-Path -LiteralPath $ReferenceShellPath).Path
} else {
    $referenceShell = $ReferenceShellPath
    $wslExecutable = (Get-Command wsl.exe -ErrorAction Stop).Source
}
$fixtureDirectory = Join-Path $PSScriptRoot "fixtures"
$cases = @(
    @{ Name = "positional arguments"; Script = "arguments.ksh"; Arguments = @("alpha beta", "gamma") },
    @{ Name = "field splitting"; Script = "field-splitting.ksh"; Arguments = @() },
    @{ Name = "command substitution"; Script = "command-substitution.ksh"; Arguments = @() },
    @{ Name = "parameter expansion"; Script = "parameter-expansion.ksh"; Arguments = @() },
    @{ Name = "parameter assignment and unset errors"; Script = "parameter-assignment-errors.ksh"; Arguments = @(); CompareStderr = $false },
    @{ Name = "parameter empty-value error"; Script = "parameter-empty-error.ksh"; Arguments = @(); CompareStderr = $false },
    @{ Name = "default parameter assignment remains unexported"; Script = "parameter-default-unexported.ksh"; Arguments = @() },
    @{ Name = "arrays and nameref"; Script = "arrays-nameref.ksh"; Arguments = @() },
    @{ Name = "sparse indexed arrays"; Script = "sparse-arrays.ksh"; Arguments = @() },
    @{ Name = "nameref function scope"; Script = "nameref-scope.ksh"; Arguments = @() },
    @{ Name = "function scope and return"; Script = "function-scope.ksh"; Arguments = @() },
    @{ Name = "compound type fields"; Script = "compound-type.ksh"; Arguments = @() },
    @{ Name = "multiline nested compound type defaults"; Script = "compound-type-multiline.ksh"; Arguments = @() },
    @{ Name = "integer arithmetic assignment"; Script = "arithmetic-assignment.ksh"; Arguments = @() },
    @{ Name = "compound field arithmetic"; Script = "arithmetic-compound-field.ksh"; Arguments = @() },
    @{ Name = "getopts"; Script = "getopts.ksh"; Arguments = @() },
    @{ Name = "getopts OPTIND reset"; Script = "getopts-reset.ksh"; Arguments = @() },
    @{ Name = "getopts missing and unknown options"; Script = "getopts-errors.ksh"; Arguments = @() },
    @{ Name = "false conditions"; Script = "false-conditions.ksh"; Arguments = @() },
    @{ Name = "subshell state isolation"; Script = "subshell-isolation.ksh"; Arguments = @() },
    @{ Name = "pipeline status"; Script = "pipeline-status.ksh"; Arguments = @() },
    @{ Name = "file redirection"; Script = "redirection.ksh"; Arguments = @() },
    @{ Name = "left-to-right redirection order"; Script = "redirection-order-portable.ksh"; Arguments = @() },
    @{ Name = "pipeline inside redirected function"; Script = "pipeline-in-redirected-function.ksh"; Arguments = @() },
    @{ Name = "exit trap"; Script = "trap.ksh"; Arguments = @() },
    @{ Name = "exit trap status"; Script = "exit-trap-status.ksh"; Arguments = @() },
    @{ Name = "print standard descriptors"; Script = "print-standard-fds.ksh"; Arguments = @() }
)

function ConvertTo-WslPath {
    param([string]$WindowsPath)

    $fullPath = [System.IO.Path]::GetFullPath($WindowsPath)
    if ($fullPath -notmatch '^([A-Za-z]):\\(.*)$') {
        throw "WSL reference paths must be on a mounted Windows drive: $WindowsPath"
    }

    $drive = $Matches[1].ToLowerInvariant()
    $pathRemainder = $Matches[2].Replace('\', '/')
    return "/mnt/$drive/$pathRemainder"
}

function Invoke-ShellCase {
    param(
        [string]$Executable,
        [string]$ScriptPath,
        [string]$WorkingDirectory,
        [string[]]$Arguments,
        [bool]$IsCrossShell,
        [string]$WslDistribution,
        [string]$WslExecutable
    )

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $isWslReference = -not $IsCrossShell -and -not [string]::IsNullOrWhiteSpace($WslDistribution)
    $startInfo.FileName = if ($isWslReference) { $WslExecutable } else { $Executable }
    $startInfo.WorkingDirectory = $WorkingDirectory
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = -not $isWslReference
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    if ($IsCrossShell) {
        $null = $startInfo.ArgumentList.Add("--no-profile")
    } elseif ($isWslReference) {
        $null = $startInfo.ArgumentList.Add("--distribution")
        $null = $startInfo.ArgumentList.Add($WslDistribution)
        $null = $startInfo.ArgumentList.Add("--cd")
        $null = $startInfo.ArgumentList.Add((ConvertTo-WslPath $WorkingDirectory))
        $null = $startInfo.ArgumentList.Add("--exec")
        $null = $startInfo.ArgumentList.Add("sh")
        $null = $startInfo.ArgumentList.Add("-c")
        $null = $startInfo.ArgumentList.Add('"$@"; exit $?')
        $null = $startInfo.ArgumentList.Add("ksh-reference")
        $null = $startInfo.ArgumentList.Add($Executable)
    }
    $resolvedScriptPath = if ($isWslReference) { ConvertTo-WslPath $ScriptPath } else { $ScriptPath }
    $null = $startInfo.ArgumentList.Add($resolvedScriptPath)
    foreach ($argument in $Arguments) {
        $null = $startInfo.ArgumentList.Add($argument)
    }

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $null = $process.Start()
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit(30000)) {
        $process.Kill()
        $process.WaitForExit()
        $process.Dispose()
        throw "Timed out running $ScriptPath with $Executable"
    }

    $result = [pscustomobject]@{
        Stdout = $stdoutTask.GetAwaiter().GetResult().Replace("`r`n", "`n")
        Stderr = $stderrTask.GetAwaiter().GetResult().Replace("`r`n", "`n")
        ExitCode = $process.ExitCode
    }
    $process.Dispose()
    return $result
}

$runRoot = if ([string]::IsNullOrWhiteSpace($ReferenceWslDistribution)) {
    [System.IO.Path]::GetTempPath()
} else {
    $PSScriptRoot
}
$runDirectory = Join-Path $runRoot (".crossshell-ksh-diff-" + [guid]::NewGuid().ToString("N"))
$passed = 0
$failed = 0

try {
    $null = New-Item -ItemType Directory -Path $runDirectory
    foreach ($case in $cases) {
        $caseDirectory = Join-Path $runDirectory ([string]$case.Script -replace "\.ksh$", "")
        $crossDirectory = Join-Path $caseDirectory "crossshell"
        $referenceDirectory = Join-Path $caseDirectory "reference"
        $null = New-Item -ItemType Directory -Path $crossDirectory -Force
        $null = New-Item -ItemType Directory -Path $referenceDirectory -Force
        $scriptPath = Join-Path $fixtureDirectory $case.Script

        $crossResult = Invoke-ShellCase $crossShell $scriptPath $crossDirectory $case.Arguments $true "" ""
        $referenceResult = Invoke-ShellCase `
            $referenceShell `
            $scriptPath `
            $referenceDirectory `
            $case.Arguments `
            $false `
            $ReferenceWslDistribution `
            $wslExecutable
        $differences = [System.Collections.Generic.List[string]]::new()
        if ($crossResult.Stdout -cne $referenceResult.Stdout) {
            $differences.Add("stdout CrossShell='$($crossResult.Stdout.Replace("`n", "\n"))' reference='$($referenceResult.Stdout.Replace("`n", "\n"))'")
        }
        if ((-not $case.ContainsKey("CompareStderr") -or $case.CompareStderr) -and $crossResult.Stderr -cne $referenceResult.Stderr) {
            $differences.Add("stderr CrossShell='$($crossResult.Stderr.Replace("`n", "\n"))' reference='$($referenceResult.Stderr.Replace("`n", "\n"))'")
        }
        if ($crossResult.ExitCode -ne $referenceResult.ExitCode) {
            $differences.Add("exit CrossShell=$($crossResult.ExitCode) reference=$($referenceResult.ExitCode)")
        }

        if ($differences.Count -eq 0) {
            Write-Host "PASS: $($case.Name)"
            $passed++
        } else {
            Write-Host "FAIL: $($case.Name): $($differences -join '; ')"
            $failed++
        }
    }
} finally {
    if (Test-Path -LiteralPath $runDirectory) {
        Remove-Item -LiteralPath $runDirectory -Recurse -Force
    }
}

Write-Host "Differential tests: $passed passed, $failed failed"
if ($failed -gt 0) {
    throw "ksh differential tests failed"
}
