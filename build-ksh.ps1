<#
.SYNOPSIS
    Convenience wrapper to build CrossShell KSH from repository root.
#>
param(
    [string]$Action = "build",
    [string]$Target = "amalgamation",
    [string]$Compiler = ""
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
& pwsh.exe -File (Join-Path $scriptDir "src\ksh\build.ps1") -Action $Action -Target $Target -Compiler $Compiler
