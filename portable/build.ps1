[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$binaryDirectory = Join-Path $repositoryRoot 'bin'
$executables = @(Get-ChildItem -LiteralPath $binaryDirectory -Filter '*.exe' -File)
if ($executables.Count -eq 0) {
    throw "No executables found in $binaryDirectory."
}

$releaseDirectory = Join-Path $repositoryRoot 'Release'
$null = New-Item -ItemType Directory -Path $releaseDirectory -Force
$outputPath = Join-Path $releaseDirectory 'CrossShell-For-UNIX-bin.zip'
$workingDirectory = Join-Path ([IO.Path]::GetTempPath()) ('crossshell-portable-' + [guid]::NewGuid().ToString('N'))
$stagingDirectory = Join-Path $workingDirectory 'payload'
$stagedBin = Join-Path $stagingDirectory 'bin'
$temporaryZip = Join-Path $workingDirectory 'portable.zip'
$null = New-Item -ItemType Directory -Path $stagedBin -Force
try {
    foreach ($executable in $executables) {
        Copy-Item -LiteralPath $executable.FullName -Destination $stagedBin
    }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Install.ps1') -Destination $stagingDirectory
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'README.txt') -Destination $stagingDirectory
    Copy-Item -LiteralPath (Join-Path $repositoryRoot 'LICENSE.txt') -Destination $stagingDirectory
    [IO.Compression.ZipFile]::CreateFromDirectory(
        $stagingDirectory, $temporaryZip, [IO.Compression.CompressionLevel]::Optimal, $false)
    Move-Item -LiteralPath $temporaryZip -Destination $outputPath -Force
    Write-Host "Created $outputPath"
    Write-Host "Embedded executables: $($executables.Count)"
} finally {
    Remove-Item -LiteralPath $workingDirectory -Recurse -Force
}
