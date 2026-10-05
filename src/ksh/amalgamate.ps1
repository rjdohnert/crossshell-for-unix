<#
.SYNOPSIS
    CrossShell KSH - SQLite-Style Amalgamation Generator
.DESCRIPTION
    Combines all modular C++17 header and source files from src/ksh/include/
    and src/ksh/src/ into a single drop-in compilation unit (src/ksh/ksh.cpp).
    Maintains 100% feature parity, eliminates duplicate includes, and adds
    clear SQLite-style file demarcation banners.
.PARAMETER OutputFile
    Target amalgamation file. Defaults to "$PSScriptRoot\ksh.cpp".
.PARAMETER SourceDir
    Root directory containing 'include' and 'src'. Defaults to "$PSScriptRoot".
#>

[CmdletBinding()]
param(
    [string]$OutputFile = "$PSScriptRoot\ksh.cpp",
    [string]$SourceDir = "$PSScriptRoot"
)

$ErrorActionPreference = "Stop"

Write-Host "CrossShell KSH Amalgamation Generator" -ForegroundColor Cyan
Write-Host "Source directory: $SourceDir"
Write-Host "Output file:      $OutputFile"

$includeDir = Join-Path $SourceDir "include"
$srcDir = Join-Path $SourceDir "src"

if (-not (Test-Path $includeDir)) {
    throw "Include directory not found: $includeDir"
}
if (-not (Test-Path $srcDir)) {
    throw "Source directory not found: $srcDir"
}

# Topological order of headers
$headerFiles = @(
    "ksh_common.h",
    "ksh_types.h",
    "ksh_state.h",
    "ksh_terminal.h",
    "ksh_traps.h",
    "ksh_variables.h",
    "ksh_io.h",
    "ksh_jobs.h",
    "ksh_parser.h",
    "ksh_expansion.h",
    "ksh_builtins.h",
    "ksh_executor.h",
    "ksh_startup.h",
    "ksh_selftest.h"
)

# Execution and definition order of source files
$sourceFiles = @(
    "ksh_state.cpp",
    "ksh_traps.cpp",
    "ksh_variables.cpp",
    "ksh_io.cpp",
    "ksh_startup.cpp",
    "ksh_parser.cpp",
    "ksh_expansion.cpp",
    "ksh_jobs.cpp",
    "ksh_terminal.cpp",
    "ksh_builtins.cpp",
    "ksh_executor.cpp",
    "ksh_selftest.cpp",
    "ksh_main.cpp"
)

$sb = [System.Text.StringBuilder]::new(2 * 1024 * 1024)

# 1. Top-Level Amalgamation Header & License
[void]$sb.AppendLine(@"
/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************
** This file is an amalgamation of all CrossShell KSH modular source files.
** It was generated automatically by amalgamate.ps1.
**
** SQLite-Style Amalgamation Design:
** - Primary development occurs modularly in src/ksh/include/ and src/ksh/src/
** - Single-file drop-in compilation is provided by this amalgamated file
** - Zero third-party runtime dependencies; standard C++17 and Win32 only
******************************************************************************/

#ifndef KSH_AMALGAMATION
#define KSH_AMALGAMATION 1
#endif
"@)

function Process-FileLines {
    param(
        [string]$FilePath,
        [bool]$IsHeader
    )

    $raw = [System.IO.File]::ReadAllText($FilePath, [System.Text.Encoding]::UTF8)
    $lines = $raw -split "\r?\n"
    $filteredLines = [System.Collections.Generic.List[string]]::new($lines.Count)

    $inLicenseComment = $false
    $pastLicense = $false

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        $trimmed = $line.Trim()

        # Strip leading BSD license comment block
        if (-not $pastLicense) {
            if ($trimmed -eq "/*" -or $trimmed.StartsWith("/*")) {
                $inLicenseComment = $true
            }
            if ($inLicenseComment) {
                if ($trimmed -eq "*/" -or $trimmed.EndsWith("*/")) {
                    $inLicenseComment = $false
                    $pastLicense = $true
                }
                continue
            }
            if ($trimmed -eq "") {
                continue
            }
            $pastLicense = $true
        }

        # Strip internal modular header includes
        if ($trimmed -match '^#\s*include\s*["<](.*[/\\])?ksh_[a-zA-Z0-9_]+\.h[">]') {
            continue
        }

        # For headers other than ksh_common.h, strip header guards
        if ($IsHeader) {
            if ($trimmed -match '^#\s*ifndef\s+CROSSSHELL_KSH_') { continue }
            if ($trimmed -match '^#\s*define\s+CROSSSHELL_KSH_') { continue }
            if ($trimmed -match '^#\s*endif\s*//\s*CROSSSHELL_KSH_') { continue }
            if ($trimmed -match '^#\s*endif\s*//\s*!\s*CROSSSHELL_KSH_') { continue }
        }

        # For source files, strip duplicate system headers (they are centralized in ksh_common.h)
        if (-not $IsHeader) {
            if ($trimmed -match '^#\s*include\s*<.*>') {
                continue
            }
            if ($trimmed -match '^#\s*pragma\s+comment\s*\(\s*lib\s*,') {
                continue
            }
        }

        $filteredLines.Add($line)
    }

    return $filteredLines
}

# 2. Process all modular headers
[void]$sb.AppendLine(@"

/*
*******************************************************************************
** SECTION I: HEADER DECLARATIONS AND TYPE DEFINITIONS
*******************************************************************************
*/
"@)

foreach ($header in $headerFiles) {
    $filePath = Join-Path $includeDir $header
    if (-not (Test-Path $filePath)) {
        throw "Header file missing: $filePath"
    }
    Write-Host "  Merging header: $header"

    [void]$sb.AppendLine("/******************************************************************************")
    [void]$sb.AppendLine("** Begin file $header")
    [void]$sb.AppendLine("******************************************************************************/")

    $processed = Process-FileLines -FilePath $filePath -IsHeader $true
    foreach ($line in $processed) {
        [void]$sb.AppendLine($line)
    }

    [void]$sb.AppendLine("/******************************************************************************")
    [void]$sb.AppendLine("** End of $header")
    [void]$sb.AppendLine("******************************************************************************/`n")
}

# 3. Process all modular implementation units
[void]$sb.AppendLine(@"

/*
*******************************************************************************
** SECTION II: IMPLEMENTATION UNITS
*******************************************************************************
*/
"@)

foreach ($src in $sourceFiles) {
    $filePath = Join-Path $srcDir $src
    if (-not (Test-Path $filePath)) {
        throw "Source file missing: $filePath"
    }
    Write-Host "  Merging source: $src"

    [void]$sb.AppendLine("/******************************************************************************")
    [void]$sb.AppendLine("** Begin file $src")
    [void]$sb.AppendLine("******************************************************************************/")

    $processed = Process-FileLines -FilePath $filePath -IsHeader $false
    foreach ($line in $processed) {
        [void]$sb.AppendLine($line)
    }

    [void]$sb.AppendLine("/******************************************************************************")
    [void]$sb.AppendLine("** End of $src")
    [void]$sb.AppendLine("******************************************************************************/`n")
}

# Write output file
$outDir = Split-Path -Parent $OutputFile
if ($outDir -and -not (Test-Path $outDir)) {
    [void][System.IO.Directory]::CreateDirectory($outDir)
}

[System.IO.File]::WriteAllText($OutputFile, $sb.ToString(), [System.Text.Encoding]::UTF8)

$fileInfo = Get-Item $OutputFile
$lineCount = ($sb.ToString() -split "`n").Count
Write-Host "`nAmalgamation successfully generated!" -ForegroundColor Green
Write-Host "File:       $($fileInfo.FullName)"
Write-Host "Size:       $([math]::Round($fileInfo.Length / 1024, 2)) KB"
Write-Host "Lines:      $lineCount"
