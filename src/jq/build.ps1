[CmdletBinding()]
param(
    [ValidateSet('auto','clang','cl')][string]$Compiler='auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference='Stop'
$ToolName='jq'
$SourceFiles=@('jq.c','jq_value.c','jq_config.c','jq_serializer.c','jq_parser.c','jq_result_set.c','jq_filter.c','jq_engine.c')
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
        Remove-Item *.obj,*.o,"$ToolName.exe",*.pdb,*.ilk -Force -ErrorAction SilentlyContinue
    }
}finally{Pop-Location}