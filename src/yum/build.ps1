[CmdletBinding()]
param(
    [ValidateSet('auto','clang','cl')][string]$Compiler='auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference='Stop'
$ToolName='yum'
$SourceFiles=@("yum.c", "command_line.c", "command_line_escaping.c", "winget_dispatch.c", "windows_update_query.c", "privilege_check.c", "windows_update_install.c", "package_install.c", "package_remove.c", "package_search.c", "package_info.c", "package_list.c", "package_upgrade.c", "repository_list.c", "repository_add.c", "repository_remove.c", "help.c", "rpm_dispatch.c", "package_clean.c")
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