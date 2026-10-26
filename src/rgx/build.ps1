[CmdletBinding()]
param(
    [ValidateSet('auto','clang','cl')][string]$Compiler='auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference='Stop'
$ToolName='rg'
$SourceFiles=@('rg.c','search_state.c','allocation.c','options.c','unicode.c','literal_matcher.c','regex_matcher.c','pattern_set.c','printer.c','text_reader.c','path_conversion.c','file_search.c','glob_matcher.c','ignore_rules.c','directory_walker.c','help.c','command_line.c','file_types.c','multiline_search.c','parallel_search.c')
Push-Location $PSScriptRoot
try {
    if($Clean){Remove-Item *.obj,*.o,"$ToolName.exe",*.pdb,*.ilk -Force -ErrorAction SilentlyContinue;return}
    if($Compiler -eq 'auto'){
        if(Get-Command clang -ErrorAction SilentlyContinue){$Compiler='clang'}
        elseif(Get-Command cl.exe -ErrorAction SilentlyContinue){$Compiler='cl'}
        else{throw 'A Windows C compiler is required: clang or cl.exe.'}
    }
    if($Compiler -eq 'clang'){
        & clang -std=c17 -O2 -Wall -Wextra @SourceFiles -o "$ToolName.exe"
    }else{
        & cl.exe /nologo /std:c17 /TC /O2 /W4 "/Fe:$ToolName.exe" @SourceFiles
    }
    if($LASTEXITCODE -ne 0){throw "Build failed for $ToolName (exit $LASTEXITCODE)."}
    if(-not $NoInstall){
        $BinDir=Join-Path $PSScriptRoot '../../bin'
        New-Item -ItemType Directory $BinDir -Force|Out-Null
        Copy-Item "$ToolName.exe" $BinDir -Force
    }
}finally{Pop-Location}