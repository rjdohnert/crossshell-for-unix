[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'Medium')]
param()

$ErrorActionPreference = 'Stop'
$destination = 'C:\Program Files\bin'
$source = Join-Path $PSScriptRoot 'bin'
$executables = @(Get-ChildItem -LiteralPath $source -Filter '*.exe' -File)
if ($executables.Count -eq 0) {
    throw "No executables found in $source. Extract the entire ZIP before installing."
}

if (-not $WhatIfPreference) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    try {
        $principal = [Security.Principal.WindowsPrincipal]::new($identity)
        if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
            throw 'Open PowerShell as administrator and run this script again.'
        }
    } finally {
        $identity.Dispose()
    }
}

if ($PSCmdlet.ShouldProcess($destination, "Install $($executables.Count) executables and add directory to system PATH")) {
    $null = New-Item -ItemType Directory -Path $destination -Force
    foreach ($executable in $executables) {
        Copy-Item -LiteralPath $executable.FullName -Destination $destination -Force
    }

    $environmentKey = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey(
        'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', $true)
    if ($null -eq $environmentKey) {
        throw 'Cannot open the system environment registry key. Files were copied, but PATH was not updated.'
    }
    try {
        $machinePath = [string]$environmentKey.GetValue(
            'Path', '', [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
        $entries = @($machinePath -split ';' | ForEach-Object {
            [Environment]::ExpandEnvironmentVariables($_.Trim().Trim('"')).TrimEnd('\')
        })
        if ($entries -notcontains $destination.TrimEnd('\')) {
            $separator = if ($machinePath -and -not $machinePath.EndsWith(';')) { ';' } else { '' }
            $kind = if ($machinePath) {
                $environmentKey.GetValueKind('Path')
            } else {
                [Microsoft.Win32.RegistryValueKind]::ExpandString
            }
            $environmentKey.SetValue('Path', "$machinePath$separator$destination", $kind)
        }
    } finally {
        $environmentKey.Dispose()
    }

    Write-Host "Installed $($executables.Count) executables to $destination."
    Write-Host 'System PATH is configured. Sign out and back in so new terminals inherit the updated PATH.'
}
