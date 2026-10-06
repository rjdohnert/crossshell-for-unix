#include "shell_generator.hpp"

void ShellGenerator::GeneratePowerShell() {
        std::cout << R"(# PowerShell Completion Module for registryctl
Register-ArgumentCompleter -Native -CommandName registryctl -ScriptBlock {
    param($wordToComplete, $commandAst, $cursorPosition)
    $commands = @(
        'get','set','delete','diff','batch','undo','completions','help',
        '--dry-run','--tx','--json','--undo-file','--view','--help'
    )
    $hives = @('HKLM\','HKCU\','HKCR\','HKU\','HKCC\')
    $types = @('REG_SZ','REG_DWORD','REG_QWORD','REG_MULTI_SZ','REG_BINARY','REG_EXPAND_SZ')

    if ($commandAst.Elements.Count -eq 2) {
        $commands | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
    } elseif ($commandAst.Elements.Count -eq 3) {
        $hives | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
    } elseif ($commandAst.Elements.Count -eq 5) {
        $types | Where-Object { $_ -like "$wordToComplete*" } | ForEach-Object {
            [System.Management.Automation.CompletionResult]::new($_, $_, 'ParameterValue', $_)
        }
    }
}
)";
    }

void ShellGenerator::GenerateZsh() {
        std::cout << R"(#compdef registryctl
_registryctl() {
    local -a commands
    commands=(
        'get:Read a registry value'
        'set:Create or update a registry value'
        'delete:Remove a registry value or subkey'
        'diff:Compare proposed registry changes'
        'batch:Execute actions from a manifest JSON file'
        'undo:Rollback actions using an undo log JSON file'
        'completions:Generate shell completion scripts'
        'help:Display manual and usage guides'
    )
    _arguments \
        '--dry-run[Simulate operation without writing changes]' \
        '--tx[Run all operations inside a transactional atomic block]' \
        '--json[Output results in JSON format]' \
        '--view=[Target 32-bit or 64-bit registry view]:view:(32 64 default)' \
        '--undo-file=[Path to export rollback journal]:file:_files' \
        '1: :->command' \
        '*:: :->args'

    case $state in
        command) _describe -t commands 'registryctl command' commands ;;
    esac
}
_registryctl "$@"
)";
    }

void ShellGenerator::GenerateKsh() {
        std::cout << R"(# KornShell (ksh) autocompletion helper for registryctl
set -A complete_registryctl -- get set delete diff batch undo completions help --dry-run --tx --json --undo-file --view
)";
    }

void ShellGenerator::GenerateCmdWrapper() {
        std::cout << R"(@echo off
:: registryctl Production CMD / Batch Wrapper
setlocal enabledelayedexpansion
if "%~1"=="" goto usage
registryctl.exe %*
exit /b %errorlevel%

:usage
echo 
echo  registryctl Batch Wrapper
echo 
echo Usage: registryctl.exe [command] [arguments...] [options]
echo Run 'registryctl help' for detailed instructions.
exit /b 1
)";
    }
