[CmdletBinding()]
param(
    [string]$Executable="$PSScriptRoot/rg.exe",
    [string]$ReferenceExecutable='',
    [ValidateRange(0,1000)][int]$FuzzCases=100,
    [switch]$NoBuild,
    [ValidateSet('auto','clang','cl')][string]$Compiler='auto'
)
$ErrorActionPreference='Stop'
if(-not $NoBuild){& "$PSScriptRoot/build.ps1" -Compiler $Compiler -NoInstall}
$Executable=[IO.Path]::GetFullPath($Executable)
if(-not(Test-Path $Executable)){throw "Executable not found: $Executable"}
if($ReferenceExecutable){$ReferenceExecutable=[IO.Path]::GetFullPath($ReferenceExecutable);if(-not(Test-Path $ReferenceExecutable)){throw 'Reference executable not found'}}
$work=Join-Path ([IO.Path]::GetTempPath()) ('rgx-regression-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $work|Out-Null
$passed=0
function Invoke-Fixture {
    param([string]$Binary,[string[]]$Arguments,[string]$InputText='',[byte[]]$InputBytes=@())
    $info=[Diagnostics.ProcessStartInfo]::new($Binary);$info.UseShellExecute=$false;$info.WorkingDirectory=$work
    $info.RedirectStandardInput=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $info.StandardOutputEncoding=[Text.Encoding]::UTF8;$info.StandardErrorEncoding=[Text.Encoding]::UTF8
    foreach($argument in $Arguments){$info.ArgumentList.Add($argument)}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info;$process.Start()|Out-Null
    $outTask=$process.StandardOutput.ReadToEndAsync();$errTask=$process.StandardError.ReadToEndAsync()
    [byte[]]$bytes=@()
    if($InputBytes.Length){$bytes=$InputBytes}
    elseif($InputText.Length){$bytes=[Text.Encoding]::UTF8.GetBytes($InputText)}
    try{$process.StandardInput.BaseStream.Write($bytes,0,$bytes.Length)}catch [IO.IOException]{}
    try{$process.StandardInput.Close()}catch [IO.IOException]{}
    if(-not $process.WaitForExit(15000)){$process.Kill($true);$process.Dispose();throw "Timeout: $($Arguments -join ' ')"}
    $result=@{Output=$outTask.GetAwaiter().GetResult();Error=$errTask.GetAwaiter().GetResult();Exit=$process.ExitCode}
    $process.Dispose();return $result
}
function Assert-True {
    param([bool]$Condition,[string]$Name)
    if(-not $Condition){throw "FAIL: $Name"}
    $script:passed++;Write-Host "PASS: $Name"
}
try {
    $cases=@(
        @{Name='greedy';Args=@('-o','a+');Input="aaaa`n";Output="aaaa`n";Exit=0},
        @{Name='lazy';Args=@('-o','a+?');Input="aaaa`n";Output="a`na`na`na`n";Exit=0},
        @{Name='alternation';Args=@('-o','(ab|a)b');Input="ab`n";Output="ab`n";Exit=0},
        @{Name='classes';Args=@('-o','[a-z]+\d{2}');Input="abc12`n";Output="abc12`n";Exit=0},
        @{Name='patterns';Args=@('-e','alpha','-e','beta');Input="alpha`nbeta`ngamma`n";Output="alpha`nbeta`n";Exit=0},
        @{Name='empty only-matching';Args=@('-o','');Input="abc`n";Output='';Exit=0},
        @{Name='context';Args=@('-n','-C1','hit');Input="before`nhit`nbetween`nhit`nafter`n";Output="1-before`n2:hit`n3-between`n4:hit`n5-after`n";Exit=0},
        @{Name='count limit';Args=@('-c','-m1','hit');Input="hit`nhit`n";Output="1`n";Exit=0},
        @{Name='invert';Args=@('-v','hit');Input="hit`nmiss`n";Output="miss`n";Exit=0},
        @{Name='CRLF';Args=@('--crlf','-x','hit');Input="hit`r`n";Output="hit`n";Exit=0},
        @{Name='malformed regex';Args=@('[');Input="abc`n";Output='';Exit=2},
        @{Name='missing argument';Args=@('-e');Input='';Output='';Exit=2},
        @{Name='negative limit';Args=@('-m-1','a');Input="a`n";Output='';Exit=2},
        @{Name='option separator';Args=@('-F','--','-token');Input="-token`n";Output="-token`n";Exit=0},
        @{Name='binary text';Args=@('-ao','abc');Input="abc`0xyz`n";Output="abc`n";Exit=0},
        @{Name='full-line KMP';Args=@('-Fx','alpha');Input="alphabet`nalpha`n";Output="alpha`n";Exit=0},
        @{Name='multiline';Args=@('-Uo','foo\nbar');Input="before`nfoo`nbar`nafter`n";Output="foo`nbar`n";Exit=0},
        @{Name='dotall';Args=@('-Uo','(?s:foo.*bar)');Input="foo`nbar`n";Output="foo`nbar`n";Exit=0},
        @{Name='absolute anchor';Args=@('-Uo','\Afoo');Input="before`nfoo`n";Output='';Exit=1},
        @{Name='offset/column';Args=@('-nbo','--column','alpha');Input="xxalpha`n";Output="1:3:2:alpha`n";Exit=0},
        @{Name='whole-match replacement';Args=@('-r','<$0>','alpha');Input="xxalpha`n";Output="xx<alpha>`n";Exit=0},
        @{Name='repeated replacement';Args=@('-r','X','alpha');Input="alpha alpha`n";Output="X X`n";Exit=0},
        @{Name='multiline count-only';Args=@('-Uco','alpha');Input="alpha alpha`n";Output="2`n";Exit=0},
        @{Name='unique multiline lines';Args=@('-Uc','alpha');Input="alpha alpha`n";Output="1`n";Exit=0},
        @{Name='literal multiline anchors';Args=@('-UFx','alpha');Input="alphabet`nalpha`n";Output="alpha`n";Exit=0},
        @{Name='Unicode property';Args=@('-o','\p{Lu}+');Input="ABC 123`n";Output="ABC`n";Exit=0}
    )
    foreach($case in $cases){$actual=Invoke-Fixture $Executable $case.Args $case.Input;Assert-True ($actual.Exit -eq $case.Exit -and $actual.Output -ceq $case.Output) $case.Name}
    $sigma=[string][char]0x3C2;$actual=Invoke-Fixture $Executable @('-Fi',([string][char]0x3A3)) ($sigma+"`n");Assert-True ($actual.Exit -eq 0 -and $actual.Output -ceq ($sigma+"`n")) 'special Unicode folding'
    $actual=Invoke-Fixture $Executable @('-Fi',([char]::ConvertFromUtf32(0x10400))) ([char]::ConvertFromUtf32(0x10428)+"`n");Assert-True ($actual.Exit -eq 0) 'supplementary case folding'
    $actual=Invoke-Fixture $Executable @('--json','alpha') "alpha alpha`n";$events=@($actual.Output -split "`n"|Where-Object {$_}|ForEach-Object {$_|ConvertFrom-Json})
    $record=@($events|Where-Object type -eq 'match')[0]
    Assert-True (($events.type -join ',') -eq 'begin,match,end,summary' -and $record.data.submatches.Count -eq 2 -and $record.data.lines.text -ceq "alpha alpha`n") 'JSON schema, all submatches, newline'
    Assert-True ($events[-1].data.stats.bytes_printed -gt 0 -and $events[-1].data.stats.matches -eq 2) 'measured JSON output statistics'
    $actual=Invoke-Fixture $Executable @('--json','-ao','.') (([string][char]0x00E9)+"`n");$null=$actual.Output -split "`n"|Where-Object {$_}|ForEach-Object {$_|ConvertFrom-Json};Assert-True ($actual.Exit -eq 0) 'valid UTF8 JSON'
    $tree=Join-Path $work 'tree';New-Item -ItemType Directory "$tree/.git","$tree/sub" -Force|Out-Null
    foreach($name in @('a.c','b.h','sub/d.c','skip.txt')){[IO.File]::WriteAllText((Join-Path $tree $name),"marker`n")}
    [IO.File]::WriteAllText("$tree/.gitignore","*.c`n");[IO.File]::WriteAllText("$tree/.ignore","!a.c`n");[IO.File]::WriteAllText("$tree/.rgignore","b.h`n")
    $actual=Invoke-Fixture $Executable @('--files',$tree);Assert-True ($actual.Output -match 'a.c' -and $actual.Output -notmatch 'b.h') 'ignore-source precedence'
    $actual=Invoke-Fixture $Executable @('--files','-g','**/*.{c,h}',$tree);Assert-True (@($actual.Output -split "`n"|Where-Object {$_}).Count -eq 3) 'brace and recursive globs'
    $actual=Invoke-Fixture $Executable @('--files','--no-ignore','-tc',$tree);Assert-True (@($actual.Output -split "`n"|Where-Object {$_}).Count -eq 3) 'builtin file type'
    $actual=Invoke-Fixture $Executable @('--files','--type-add','custom:*.txt','-tcustom',$tree);Assert-True ($actual.Output -match 'skip.txt') 'custom file type'
    $actual=Invoke-Fixture $Executable @('--files','--no-ignore','-Tall',$tree);Assert-True ($actual.Exit -eq 1) 'file type exclusion'
    $actual=Invoke-Fixture $Executable @('--type-clear','c','--type-list');Assert-True ($actual.Output -notmatch '(?m)^c:') 'builtin type clearing'
    $actual=Invoke-Fixture $Executable @('--type-list');Assert-True ($actual.Output -match 'powershell:') 'type listing'
    $actual=Invoke-Fixture $Executable @('--files','--null',$tree);Assert-True ($actual.Output.Contains([string][char]0)) 'NUL path delimiters'
    $actual=Invoke-Fixture $Executable @('--heading','marker',"$tree/a.c");Assert-True ($actual.Output.StartsWith("$tree/a.c")) 'heading output'
    $outside=Join-Path $work 'outside';New-Item -ItemType Directory $outside|Out-Null
    [IO.File]::WriteAllText("$outside/one.txt","marker`n");[IO.File]::WriteAllText("$outside/.gitignore","one.txt`n")
    $actual=Invoke-Fixture $Executable @('--files',$outside);Assert-True ($actual.Output -match 'one.txt') 'Git ignores require repository'
    $actual=Invoke-Fixture $Executable @('--files','--no-require-git',$outside);Assert-True ($actual.Exit -eq 1) 'explicit nonrepository Git ignores'
    $globalIgnore=Join-Path $work 'global-ignore';[IO.File]::WriteAllText($globalIgnore,"skip.txt`n")
    $globalConfig=Join-Path $work 'gitconfig';[IO.File]::WriteAllText($globalConfig,"[core]`n excludesFile = $globalIgnore`n")
    $oldConfig=$env:GIT_CONFIG_GLOBAL
    try{$env:GIT_CONFIG_GLOBAL=$globalConfig;$actual=Invoke-Fixture $Executable @('--files',$tree);Assert-True ($actual.Output -notmatch 'skip.txt') 'native global Git excludes'}finally{$env:GIT_CONFIG_GLOBAL=$oldConfig}
    $parallel=Join-Path $work 'parallel';New-Item -ItemType Directory $parallel|Out-Null
    foreach($index in 1..80){[IO.File]::WriteAllText((Join-Path $parallel "$index.txt"),"alpha alpha`n")}
    $serial=Invoke-Fixture $Executable @('-j1','--count-matches','alpha',$parallel);$workers=Invoke-Fixture $Executable @('-j4','--count-matches','alpha',$parallel)
    Assert-True ((($serial.Output -split "`n"|Sort-Object)-join "`n") -ceq (($workers.Output -split "`n"|Sort-Object)-join "`n") -and $workers.Exit -eq 0) 'bounded worker queue and serial agreement'
    $actual=Invoke-Fixture $Executable @('-j4','--json','alpha',$parallel);$events=@($actual.Output -split "`n"|Where-Object {$_}|ForEach-Object {$_|ConvertFrom-Json});Assert-True (@($events|Where-Object type -eq 'match').Count -eq 80 -and $events[-1].type -eq 'summary') 'parallel atomic JSON'
    Assert-True ($events[-1].data.stats.searches_with_match -eq 80 -and $events[-1].data.stats.matches -eq 160) 'parallel statistics merging'
    $large=Join-Path $work 'large.txt';[IO.File]::WriteAllText($large,('a'*1000000)+"b`n")
    $timer=[Diagnostics.Stopwatch]::StartNew();$actual=Invoke-Fixture $Executable @('-Fco',(('a'*1024)+'b'),$large);$timer.Stop();Assert-True ($actual.Exit -eq 0 -and $actual.Output -ceq "1`n" -and $timer.Elapsed.TotalSeconds -lt 10) 'linear literal throughput gate'
    $utf16=Join-Path $work 'utf16.txt';[IO.File]::WriteAllText($utf16,"marker`r`n",[Text.Encoding]::Unicode);$actual=Invoke-Fixture $Executable @('--crlf','marker',$utf16);Assert-True ($actual.Output -ceq "marker`n") 'buffered UTF16 BOM'
    $invalid=Join-Path $work 'invalid.txt';[IO.File]::WriteAllBytes($invalid,[byte[]]@(255,254,0,216));$actual=Invoke-Fixture $Executable @('a',$invalid);Assert-True ($actual.Exit -eq 2) 'invalid UTF16 error'
    $random=[Random]::new(20261006);$atoms=@('a','b','.','[ab]','[^x]','\d','\w','\p{L}','(a|b)','(',']','\\')
    foreach($index in 1..$FuzzCases){if(!$FuzzCases){break};$pattern='';foreach($part in 1..$random.Next(1,10)){$pattern+=$atoms[$random.Next($atoms.Count)];if($random.Next(4) -eq 0){$pattern+='*'}};$actual=Invoke-Fixture $Executable @('-o',$pattern) "aaaa bbbb 123`n";if($actual.Exit -notin @(0,1,2)){throw "Fuzz crash: $pattern"}}
    Assert-True $true "deterministic bounded regex fuzz ($FuzzCases cases)"
    foreach($index in 1..$FuzzCases){
        if(!$FuzzCases){break}
        $dataBytes=[byte[]]::new($random.Next(0,129));$random.NextBytes($dataBytes)
        if($index%3 -eq 0){$dataBytes=[byte[]]@(255,254)+$dataBytes}
        $actual=Invoke-Fixture $Executable @('-a','--json','.') -InputBytes $dataBytes
        if($actual.Exit -notin @(0,1,2)){throw 'Encoding fuzz crash'}
        $null=$actual.Output -split "`n"|Where-Object {$_}|ForEach-Object {$_|ConvertFrom-Json}
    }
    Assert-True $true "deterministic UTF8/UTF16 byte fuzz ($FuzzCases cases)"
    if($ReferenceExecutable){foreach($case in $cases|Where-Object {$_.Name -notin @('malformed regex','missing argument','negative limit')}){$ours=Invoke-Fixture $Executable $case.Args $case.Input;$reference=Invoke-Fixture $ReferenceExecutable $case.Args $case.Input;Assert-True ($ours.Output -ceq $reference.Output -and $ours.Exit -eq $reference.Exit) ("reference: "+$case.Name)}}
    else{Write-Host 'SKIP: reference ripgrep comparisons (supply -ReferenceExecutable)'}
    Write-Host "RGX regression summary: $passed checks passed."
}finally{Remove-Item $work -Recurse -Force}