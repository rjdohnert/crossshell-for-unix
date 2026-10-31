[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl', 'gcc')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
$ToolName = 'wget'
$SourceFiles = @("wget.c")
Push-Location $PSScriptRoot
try {
    if ($Clean) {
        Remove-Item *.obj, *.o, "$ToolName.exe", *.pdb, *.ilk -Force -ErrorAction SilentlyContinue
        return
    }
    if ($Compiler -eq 'auto') {
        if (Get-Command clang -ErrorAction SilentlyContinue) { $Compiler = 'clang' }
        elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $Compiler = 'cl' }
        elseif (Get-Command gcc -ErrorAction SilentlyContinue) { $Compiler = 'gcc' }
        else { throw 'No suitable C compiler found (clang, cl, or gcc required).' }
    }
    switch ($Compiler) {
        'clang' { & clang -std=c99 -O2 -o "$ToolName.exe" @SourceFiles -lwinhttp  }
        'cl' { & cl.exe /nologo /std:c17 /O2 "/Fe:$ToolName.exe" @SourceFiles winhttp.lib  }
        'gcc' { & gcc -std=c99 -O2   -o "$ToolName.exe" @SourceFiles -lwinhttp  }
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $ToolName (exit $LASTEXITCODE)." }
    Write-Host "[$ToolName] Build successful: $ToolName.exe"
    if (-not $NoInstall) {
        $BinDir = Join-Path $PSScriptRoot '../../bin'
        New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
        Copy-Item "$ToolName.exe" $BinDir -Force
        Write-Host "[$ToolName] Installed to $BinDir\$ToolName.exe"
    }
} finally { Pop-Location }
