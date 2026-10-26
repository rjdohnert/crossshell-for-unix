[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl')][string]$Compiler = 'auto'
)

$ErrorActionPreference = 'Stop'
$buildScript = Join-Path $PSScriptRoot 'build.ps1'
& $buildScript -Compiler $Compiler -NoInstall
if ($LASTEXITCODE -ne 0) { throw 'ls build failed.' }

$ls = Join-Path $PSScriptRoot 'ls.exe'
$root = Join-Path $env:TEMP ('ls-test-' + [guid]::NewGuid().ToString('N'))
$nested = Join-Path $root 'nested'
New-Item -ItemType Directory -Path $nested -Force | Out-Null

function Assert-True([bool]$condition, [string]$message) {
    if (-not $condition) { throw $message }
}

function Invoke-LsUtf8([string[]]$Arguments) {
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $ls
    $info.UseShellExecute = $false
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.StandardOutputEncoding = [System.Text.UTF8Encoding]::new($false)
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = [System.Diagnostics.Process]::Start($info)
    $output = $process.StandardOutput.ReadToEnd()
    $errorOutput = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    return [pscustomobject]@{
        ExitCode = $process.ExitCode
        Output = $output
        Error = $errorOutput
    }
}

try {
    $source = Join-Path $root 'sample.c'
    $archive = Join-Path $root 'sample.zip'
    $installer = Join-Path $root 'setup.msi'
    $media = Join-Path $root 'sound.mp3'
    $image = Join-Path $root 'picture.png'
    $large = Join-Path $root 'large.bin'
    $leaf = Join-Path $nested 'leaf.txt'
    $spaceName = Join-Path $root 'space name.txt'
    $apostropheName = Join-Path $root "can't.txt"
    $specialName = Join-Path $root 'shell & (name).txt'
    $unicodeName = Join-Path $root '界-source.c'
    $version2 = Join-Path $root 'item2.txt'
    $version10 = Join-Path $root 'item10.txt'
    $extensionA = Join-Path $root 'alpha.z'
    $extensionB = Join-Path $root 'beta.c'
    foreach ($path in @($source, $archive, $installer, $media, $image, $leaf,
                        $spaceName, $apostropheName, $specialName, $unicodeName,
                        $version2, $version10, $extensionA, $extensionB)) {
        New-Item -ItemType File -Path $path | Out-Null
    }
    $stream = [System.IO.File]::OpenWrite($large)
    try { $stream.SetLength(1MB) } finally { $stream.Dispose() }
    0..17 | ForEach-Object {
        New-Item -ItemType File -Path (Join-Path $root ('entry-{0:D2}.txt' -f $_)) | Out-Null
    }
    New-Item -ItemType File -Path (Join-Path $root '.hidden') | Out-Null

    $recursive = (& $ls -R -1 $root) -join "`n"
    Assert-True ($LASTEXITCODE -eq 0 -and $recursive.Contains('leaf.txt') -and
                 $recursive.Contains("${nested}:")) '-R did not traverse nested directories.'

    $wildcard = Join-Path $root '*.c'
    $wildcardOutput = (& $ls -1 $wildcard) -join "`n"
    Assert-True ($LASTEXITCODE -eq 0 -and $wildcardOutput.Contains('sample.c')) 'Wildcard expansion failed.'

    $noMatch = Join-Path $root 'missing-*.none'
    $null = & $ls -1 $noMatch 2>$null
    Assert-True ($LASTEXITCODE -ne 0) 'An unmatched wildcard should return failure.'

    $human = (& $ls -lh $large) -join "`n"
    Assert-True ($human -match '1\.0M') '-h did not format a 1 MiB file as 1.0M.'

    $inode = (& $ls -i $source) -join "`n"
    Assert-True ($inode -match '^\d+\s') '-i did not display a Windows file ID.'

    $blocks = (& $ls -s $large) -join "`n"
    Assert-True ($blocks -match '^\s*\d+\s') '-s did not display allocation blocks.'

    $defaultOutput = (& $ls --no-color $root) -join "`n"
    Assert-True ($defaultOutput -match '(?m)^sample\.c$') 'Redirected default output was not one entry per line.'

    $singleFile = (& $ls --no-color $source) -join "`n"
    Assert-True ($singleFile -eq 'sample.c') 'Single-file listing emitted extra blank lines.'

    $recursiveFile = (& $ls -R --no-color $source) -join "`n"
    Assert-True ($recursiveFile -eq 'sample.c') '-R incorrectly added a directory heading for a file.'

    $commaOutput = (& $ls --no-color -m $root) -join "`n"
    Assert-True ($commaOutput.Contains(', ')) '-m did not separate entries with commas.'

    $columnOutput = (& $ls --no-color -C $root) -join "`n"
    $acrossOutput = (& $ls --no-color -x $root) -join "`n"
    Assert-True ($columnOutput -ne $acrossOutput) '-C and -x did not use distinct traversal orders.'

    $singleThenColumns = @(& $ls --no-color -1C $root)
    $columnsThenSingle = @(& $ls --no-color -C1 $root)
    Assert-True ($singleThenColumns.Count -lt $columnsThenSingle.Count) 'Layout flags did not honor last-option precedence.'

    $cQuote = Invoke-LsUtf8 @('-Q', '--no-color', $spaceName)
    Assert-True ($cQuote.ExitCode -eq 0 -and $cQuote.Output.Trim() -eq '"space name.txt"') 'C-style quoting failed.'

    $shellQuote = Invoke-LsUtf8 @('--quoting-style=shell', '--no-color', $apostropheName)
    Assert-True ($shellQuote.ExitCode -eq 0 -and $shellQuote.Output.Trim() -eq "'can'\''t.txt'") 'Shell quoting failed.'

    $escapedName = Invoke-LsUtf8 @('-b', '--no-color', $specialName)
    Assert-True ($escapedName.ExitCode -eq 0 -and $escapedName.Output.Trim() -eq 'shell\ \&\ \(name\).txt') 'Shell-special escaping failed.'

    $unicodeOutput = Invoke-LsUtf8 @('-N', '--no-color', $unicodeName)
    Assert-True ($unicodeOutput.ExitCode -eq 0 -and $unicodeOutput.Output.Trim() -eq '界-source.c') 'UTF-8 filename output failed.'

    $widthRoot = Join-Path $root 'width'
    New-Item -ItemType Directory -Path $widthRoot | Out-Null
    New-Item -ItemType File -Path (Join-Path $widthRoot '界界界.txt') | Out-Null
    New-Item -ItemType File -Path (Join-Path $widthRoot 'abcdefgh') | Out-Null
    $narrowColumns = Invoke-LsUtf8 @('--no-color', '-C', '--width=20', $widthRoot)
    $narrowLines = @($narrowColumns.Output.TrimEnd("`n", "`r") -split "`r?`n")
    Assert-True ($narrowColumns.ExitCode -eq 0 -and $narrowLines.Count -eq 2) 'Unicode display width did not constrain narrow columns correctly.'

    $fastOutput = (& $ls --no-color -f $root) -join "`n"
    Assert-True ($fastOutput.Contains('.hidden')) '-f did not include dotfiles.'

    $versionOutput = (& $ls --no-color -1 -v $root) -join "`n"
    Assert-True ($versionOutput.IndexOf('item2.txt') -lt $versionOutput.IndexOf('item10.txt')) '-v did not use natural numeric ordering.'

    $extensionOutput = (& $ls --no-color -1 -X $root) -join "`n"
    Assert-True ($extensionOutput.IndexOf('beta.c') -lt $extensionOutput.IndexOf('alpha.z')) '-X did not sort by extension.'

    $groupedOutput = (& $ls --no-color -1 --group-directories-first $root) -join "`n"
    Assert-True ($groupedOutput.StartsWith('nested')) 'Directory-first grouping failed.'
    $groupedOutput = (& $ls --no-color -1 --group-directories-last $root) -join "`n"
    $groupedLines = @($groupedOutput -split "`n")
    $firstDirectoryIndex = [Math]::Min($groupedLines.IndexOf('nested'), $groupedLines.IndexOf('width'))
    Assert-True ($groupedLines.IndexOf('beta.c') -lt $firstDirectoryIndex) 'Directory-last grouping failed.'

    $acl = (& $ls --acl $source) -join "`n"
    Assert-True ($LASTEXITCODE -eq 0 -and $acl -match 'ALLOW\s+[rwx-]{3}') '--acl did not display DACL entries.'

    $escape = [char]27
    $colorCases = @(
        @{ Path = $root; Flag = '-d'; Code = "${escape}[94m" },
        @{ Path = $source; Flag = ''; Code = "${escape}[38;5;208m" },
        @{ Path = $archive; Flag = ''; Code = "${escape}[91m" },
        @{ Path = $installer; Flag = ''; Code = "${escape}[96m" },
        @{ Path = $media; Flag = ''; Code = "${escape}[35m" },
        @{ Path = $image; Flag = ''; Code = "${escape}[38;5;213m" }
    )
    foreach ($case in $colorCases) {
        $colored = if ($case.Flag) { (& $ls --color $case.Flag $case.Path) -join "`n" }
                   else { (& $ls --color $case.Path) -join "`n" }
        Assert-True ($colored.Contains($case.Code)) "Expected color code missing for $($case.Path)."
    }

    $plain = (& $ls --no-color $source) -join "`n"
    Assert-True (-not $plain.Contains($escape)) '--no-color emitted ANSI escapes.'

    $oldLsColors = $env:LS_COLORS
    try {
        $env:LS_COLORS = '*.c=31:di=36'
        $customSource = Invoke-LsUtf8 @('--color', $source)
        $customDirectory = Invoke-LsUtf8 @('--color', '-d', $root)
        Assert-True ($customSource.Output.Contains("${escape}[31m")) 'LS_COLORS extension override failed.'
        Assert-True ($customDirectory.Output.Contains("${escape}[36m")) 'LS_COLORS directory override failed.'
    } finally {
        if ($null -eq $oldLsColors) { Remove-Item Env:LS_COLORS -ErrorAction SilentlyContinue }
        else { $env:LS_COLORS = $oldLsColors }
    }

    $linkTarget = Join-Path $root 'link-target'
    New-Item -ItemType Directory -Path $linkTarget | Out-Null
    New-Item -ItemType File -Path (Join-Path $linkTarget 'linked.txt') | Out-Null
    $junction = Join-Path $root 'junction'
    & $env:ComSpec /d /c mklink /J $junction $linkTarget 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $cycle = Join-Path $linkTarget 'cycle'
        & $env:ComSpec /d /c mklink /J $cycle $root 2>$null | Out-Null
        $noFollow = (& $ls --no-color -R -1 $root) -join "`n"
        $followAll = (& $ls --no-color -L -R -1 $root) -join "`n"
        $followTarget = (& $ls --no-color -H -R -1 $junction) -join "`n"
        Assert-True ([regex]::Matches($noFollow, 'linked\.txt').Count -eq 1) 'Default traversal followed a junction.'
        Assert-True ([regex]::Matches($followAll, 'linked\.txt').Count -eq 1) '-L duplicated or looped while following junctions.'
        Assert-True ($followTarget.Contains('linked.txt')) '-H did not follow a command-line junction.'
    }
    'PASS: recursion, wildcards, quoting, Unicode widths, layouts, sizes, IDs, ACLs, and colors.'
} finally {
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}