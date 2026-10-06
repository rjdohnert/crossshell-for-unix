[CmdletBinding()]
param(
    [ValidateSet('auto', 'clang', 'cl', 'gcc')]
    [string]$Compiler = 'auto',
    [switch]$Clean,
    [switch]$NoInstall
)
$ErrorActionPreference = 'Stop'
$ToolName = 'zip'
$SourceFiles = @("archive_creator.cpp", "archive_extension.cpp", "compression_level.cpp", "option_parser.cpp", "output_buffer.cpp", "output_session.cpp", "pipe_buffer.cpp", "powershell_quoting.cpp", "zip_app.cpp", "zip_help.cpp", "zip.cpp")
Push-Location $PSScriptRoot
try {
    if ($Clean) {
        Remove-Item *.obj, *.o, "$ToolName.exe", *.pdb, *.ilk -Force -ErrorAction SilentlyContinue
        return
    }
    if ($Compiler -eq 'auto') {
        if (Get-Command clang++ -ErrorAction SilentlyContinue) { $Compiler = 'clang' }
        elseif (Get-Command cl.exe -ErrorAction SilentlyContinue) { $Compiler = 'cl' }
        elseif (Get-Command g++ -ErrorAction SilentlyContinue) { $Compiler = 'gcc' }
        else { throw 'No suitable C++ compiler found (clang++, cl, or g++ required).' }
    }
    switch ($Compiler) {
        'clang' { & clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "$ToolName.exe" @SourceFiles  }
        'cl' { & cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:$ToolName.exe" @SourceFiles  }
        'gcc' { & g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX   -o "$ToolName.exe" @SourceFiles  }
    }
    if ($LASTEXITCODE -ne 0) { throw "Build failed for $ToolName (exit $LASTEXITCODE)." }
    if (-not $NoInstall) {
        $BinDir = Join-Path $PSScriptRoot '../../bin'
        New-Item -ItemType Directory -Path $BinDir -Force | Out-Null
        Copy-Item "$ToolName.exe" $BinDir -Force
    }
} finally { Pop-Location }
