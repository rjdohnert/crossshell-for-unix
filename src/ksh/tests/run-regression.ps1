param(
    [string]$ExecutablePath = (Join-Path $PSScriptRoot "..\ksh.exe")
)

$ErrorActionPreference = "Stop"
$resolvedExecutable = (Resolve-Path -LiteralPath $ExecutablePath).Path
$fixtureDirectory = Join-Path $PSScriptRoot "fixtures"
$cases = @(
    @{
        Name = "quoted positional arguments"
        Script = "arguments.ksh"
        Arguments = @("alpha beta", "gamma")
        Stdout = "alpha beta|gamma|2`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "quoted and unquoted field splitting"
        Script = "field-splitting.ksh"
        Arguments = @()
        Stdout = "unquoted:2:alpha:beta`nquoted:1:alpha beta:`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "command substitution"
        Script = "command-substitution.ksh"
        Arguments = @()
        Stdout = "<captured>`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "parameter assignment and unset operators"
        Script = "parameter-assignment-errors.ksh"
        Arguments = @()
        Stdout = "no-colon:assigned:assigned::`ncolon:assigned:assigned:assigned-empty:assigned-empty`nempty-question:`n"
        Stderr = "ksh: missing-required`n"
        ExitCode = 1
    },
    @{
        Name = "default parameter assignment stays unexported"
        Script = "parameter-default-unexported.ksh"
        Arguments = @()
        Stdout = "secret-marker`nchild:0`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "parameter empty-value error operator"
        Script = "parameter-empty-error.ksh"
        Arguments = @()
        Stdout = ""
        Stderr = "ksh: empty-required`n"
        ExitCode = 1
    },
    @{
        Name = "indexed and associative arrays with nameref"
        Script = "arrays-nameref.ksh"
        Arguments = @()
        Stdout = "apple:pear:navy:after`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "function scope and return status"
        Script = "function-scope.ksh"
        Arguments = @()
        Stdout = "outer:7`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "false if and while conditions"
        Script = "false-conditions.ksh"
        Arguments = @()
        Stdout = "false-branch`nafter`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "loop and arithmetic expansion"
        Script = "shell-flow.ksh"
        Arguments = @()
        Stdout = "world:4`nworld:9`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "compound field arithmetic assignment"
        Script = "arithmetic-compound-field.ksh"
        Arguments = @()
        Stdout = "count:7:square:49`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "pipeline status and pipefail"
        Script = "pipeline-status.ksh"
        Arguments = @()
        Stdout = "default:0`npipefail:1`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "continue after command failure"
        Script = "continue-after-failure.ksh"
        Arguments = @()
        Stdout = "continued`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "errexit stops after command failure"
        Script = "errexit.ksh"
        Arguments = @()
        Stdout = ""
        Stderr = ""
        ExitCode = 1
    },
    @{
        Name = "external pipeline"
        Script = "pipeline.ksh"
        Arguments = @()
        Stdout = "alpha`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "input and output redirection"
        Script = "redirection.ksh"
        Arguments = @()
        Stdout = "redirected`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "append redirection"
        Script = "redirection-append.ksh"
        Arguments = @()
        Stdout = "first`n1`n2`n3`n4`n5`n6`n7`n8`n9`n10`n11`n12`n13`n14`n15`n16`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "UTF-8 redirected builtin output"
        Script = "redirection-unicode.ksh"
        Arguments = @()
        Stdout = ""
        Stderr = ""
        ExitCode = 0
        Files = @{
            "unicode.txt" = "$([char]0x63)$([char]0x61)$([char]0x66)$([char]0xe9) $([char]0x6f22)$([char]0x5b57)`n"
        }
    },
    @{
        Name = "failed redirection reports status and continues"
        Script = "redirection-open-failure.ksh"
        Arguments = @()
        Stdout = "status:1`n"
        Stderr = "ksh: redirection setup failed`n"
        ExitCode = 0
    },
    @{
        Name = "simple stdout and stderr duplication"
        Script = "redirection-duplication.ksh"
        Arguments = @()
        Stdout = "stdout`n"
        Stderr = "stderr`n"
        ExitCode = 0
    },
    @{
        Name = "print to persistent custom descriptor"
        Script = "print-custom-fd.ksh"
        Arguments = @()
        Stdout = "custom-fd-output`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "print to closed custom descriptor"
        Script = "print-closed-fd.ksh"
        Arguments = @()
        Stdout = "status:1`n"
        Stderr = "ksh: print: file descriptor is not open: 9`n"
        ExitCode = 0
    },
    @{
        Name = "stderr duplicated to stdout for external command"
        Script = "stderr-to-stdout.ksh"
        Arguments = @()
        Stdout = "OUT`nERR `n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "left-to-right mixed redirection ordering"
        Script = "redirection-order.ksh"
        Arguments = @()
        Stdout = "ERR `nOUT`nOUT`nERR `n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "builtin and function streams honor redirection order"
        Script = "builtin-redirection-order.ksh"
        Arguments = @()
        Stdout = "ksh: cd failed: KSH_MISSING_REDIRECTION_PATH`nksh: cd failed: KSH_MISSING_REDIRECTION_PATH`nksh: cd failed: KSH_MISSING_REDIRECTION_PATH`nfunction-out`nksh: cd failed: KSH_MISSING_REDIRECTION_PATH`nfunction-out`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "builtin diagnostic redirection in pipeline stage"
        Script = "pipeline-builtin-redirection.ksh"
        Arguments = @()
        Stdout = "ksh: cd failed: KSH_MISSING_REDIRECTION_PATH`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "three-stage builtin redirection pipeline"
        Script = "pipeline-three-stage-redirection.ksh"
        Arguments = @()
        Stdout = "ksh: cd failed: KSH_MISSING_REDIRECTION_PATH`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "final builtin redirection in pipeline"
        Script = "pipeline-final-builtin-redirection.ksh"
        Arguments = @()
        Stdout = "last-stage`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "pipeline inside redirected function"
        Script = "pipeline-in-redirected-function.ksh"
        Arguments = @()
        Stdout = "file:read:kept`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "exit trap"
        Script = "trap.ksh"
        Arguments = @()
        Stdout = "body`nexit-trap`n"
        Stderr = ""
        ExitCode = 0
    },
    @{
        Name = "script exit status"
        Script = "exit-status.ksh"
        Arguments = @()
        Stdout = ""
        Stderr = ""
        ExitCode = 7
    },
    @{
        Name = "diagnostic and failure status"
        Script = "unsupported-option.ksh"
        Arguments = @()
        Stdout = ""
        Stderr = "ksh: set: unsupported option: unsupported`n"
        ExitCode = 1
    },
    @{
        Name = "built-ins parity and execution"
        Script = "builtins-parity.ksh"
        Arguments = @()
        Stdout = "count=15:ratio=2.5`ntotal=8`nnofile=4096`nhello from fd 4`ntest_v:ok`nprint_dash_dash:ok`narith_fix:-1:1:32`nheredoc:ok`n"
        Stderr = ""
        ExitCode = 0
    }
)

$runDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("crossshell-ksh-tests-" + [guid]::NewGuid().ToString("N"))
$passed = 0
$failed = 0

try {
    $null = New-Item -ItemType Directory -Path $runDirectory

    foreach ($case in $cases) {
        $caseDirectory = Join-Path $runDirectory ([string]$case.Script -replace "\.ksh$", "")
        $null = New-Item -ItemType Directory -Path $caseDirectory

        $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
        $startInfo.FileName = $resolvedExecutable
        $startInfo.WorkingDirectory = $caseDirectory
        $startInfo.UseShellExecute = $false
        $startInfo.CreateNoWindow = $true
        $startInfo.RedirectStandardOutput = $true
        $startInfo.RedirectStandardError = $true
        if ($startInfo.ArgumentList) {
            $null = $startInfo.ArgumentList.Add("--no-profile")
            $null = $startInfo.ArgumentList.Add((Join-Path $fixtureDirectory $case.Script))
            foreach ($argument in $case.Arguments) {
                $null = $startInfo.ArgumentList.Add($argument)
            }
        } else {
            $argItems = @("--no-profile", (Join-Path $fixtureDirectory $case.Script)) + @($case.Arguments)
            $escaped = @($argItems | ForEach-Object {
                if ($_ -match '[\s"]') { '"{0}"' -f ($_ -replace '"', '\"') } else { $_ }
            })
            $startInfo.Arguments = $escaped -join " "
        }

        $process = [System.Diagnostics.Process]::new()
        $process.StartInfo = $startInfo
        $null = $process.Start()
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()

        if (-not $process.WaitForExit(30000)) {
            $process.Kill()
            $process.WaitForExit()
            throw "Timed out running $($case.Name)"
        }

        $actualStdout = $stdoutTask.GetAwaiter().GetResult().Replace("`r`n", "`n")
        $actualStderr = $stderrTask.GetAwaiter().GetResult().Replace("`r`n", "`n")
        $failures = [System.Collections.Generic.List[string]]::new()

        if ($actualStdout -cne $case.Stdout) {
            $failures.Add("stdout expected '$($case.Stdout.Replace("`n", "\n"))', got '$($actualStdout.Replace("`n", "\n"))'")
        }
        if ($actualStderr -cne $case.Stderr) {
            $failures.Add("stderr expected '$($case.Stderr.Replace("`n", "\n"))', got '$($actualStderr.Replace("`n", "\n"))'")
        }
        if ($process.ExitCode -ne $case.ExitCode) {
            $failures.Add("exit code expected $($case.ExitCode), got $($process.ExitCode)")
        }
        foreach ($relativePath in $case.Files.Keys) {
            $expectedPath = Join-Path $caseDirectory $relativePath
            if (-not (Test-Path -LiteralPath $expectedPath)) {
                $failures.Add("expected file '$relativePath' was not created")
                continue
            }
            $actualContents = [System.IO.File]::ReadAllText($expectedPath, [System.Text.Encoding]::UTF8)
            if ($actualContents -cne $case.Files[$relativePath]) {
                $failures.Add("file '$relativePath' expected '$($case.Files[$relativePath].Replace("`n", "\n"))', got '$($actualContents.Replace("`n", "\n"))'")
            }
        }

        if ($failures.Count -eq 0) {
            Write-Host "PASS: $($case.Name)"
            $passed++
        } else {
            Write-Host "FAIL: $($case.Name): $($failures -join '; ')"
            $failed++
        }
        $process.Dispose()
    }
} finally {
    if (Test-Path -LiteralPath $runDirectory) {
        Remove-Item -LiteralPath $runDirectory -Recurse -Force
    }
}

Write-Host "Regression tests: $passed passed, $failed failed"
if ($failed -gt 0) {
    throw "ksh regression tests failed"
}

