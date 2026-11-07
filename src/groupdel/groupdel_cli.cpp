#include "groupdel_cli.hpp"

#include <iostream>

bool CommandLineParser::Parse(int argc, wchar_t* argv[],
                              CommandOptions& options,
                              std::wstring& error) {
    options = CommandOptions{};
    error.clear();
    bool positionalOnly = false;

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) {
            error = L"invalid null argument";
            return false;
        }
        const std::wstring argument = argv[i];
        if (argument.empty()) {
            error = L"empty arguments are not valid";
            return false;
        }

        if (!positionalOnly && argument == L"--") {
            positionalOnly = true;
            continue;
        }
        if (!positionalOnly &&
            (argument == L"-h" || argument == L"--help" ||
             argument == L"/?" || argument == L"-?")) {
            options.showHelp = true;
            return true;
        }
        if (!positionalOnly &&
            (argument == L"-V" || argument == L"--version")) {
            options.showVersion = true;
            return true;
        }
        if (!positionalOnly &&
            (argument == L"-f" || argument == L"--force")) {
            options.force = true;
            continue;
        }
        if (!positionalOnly &&
            (argument == L"-q" || argument == L"--quiet")) {
            options.quiet = true;
            continue;
        }
        if (!positionalOnly &&
            (argument == L"-v" || argument == L"--verbose")) {
            options.verbose = true;
            continue;
        }
        if (!positionalOnly && argument == L"-") {
            if (!options.groupNames.empty() || options.readFromStdin) {
                error = L"standard input cannot be combined with group operands";
                return false;
            }
            options.readFromStdin = true;
            continue;
        }
        if (!positionalOnly && argument[0] == L'-') {
            error = L"unrecognized option";
            return false;
        }
        if (options.readFromStdin) {
            error = L"standard input cannot be combined with group operands";
            return false;
        }
        options.groupNames.push_back(argument);
    }
    return true;
}

void CommandLineParser::PrintHelp() {
    std::wcout << LR"(groupdel(1)             CrossShell for UNIX Reference Manual              groupdel(1)

NAME
    groupdel - delete local Windows groups

SYNOPSIS
    groupdel [OPTIONS] GROUP...
    groupdel [OPTIONS] -
    type GROUPS.txt | groupdel [OPTIONS]
    Get-Content GROUPS.txt | groupdel [OPTIONS]

DESCRIPTION
    Deletes local groups from the Windows Security Accounts Manager (SAM).
    This command does not delete Active Directory groups.

OPTIONS
    -f, --force
        Succeed when a requested group does not exist. Does not bypass
        permission checks or protection for built-in groups.

    -q, --quiet
        Suppress progress and success messages. Errors are still reported.

    -v, --verbose
        Display progress and the result of each operation.

    -h, --help, /?, -?
        Display this help.

    -V, --version
        Display version information.

    --
        Treat remaining arguments as group names, including names beginning
        with a hyphen.

    -
        Read UTF-8 group records from standard input.

INPUT RECORDS
    Each nonblank line contains a group name, optionally followed by a comma
    or tab and a description. Quoted CSV fields use doubled quotes. Lines whose
    first non-whitespace character is # are comments. A UTF-8 BOM is accepted.
    Records are limited to 64 KiB.

EXIT STATUS
    0    All requested operations succeeded.
    2    Invalid options, operands, or input records.
    4    Windows account-management or input/output failure.
    6    A group does not exist and -f was not specified.
    8    A built-in or protected group cannot be deleted.
    10   Access denied.

EXAMPLES
    groupdel Developers
        Delete a local group.

    groupdel -f OldDevelopers
        Succeed if the group is already absent.

    Get-Content .\groups.txt | groupdel -v
        Delete one local group per UTF-8 input record.

CrossShell for UNIX                                                       groupdel(1)
)";
}

void CommandLineParser::PrintVersion() {
    std::wcout << L"groupdel 3.0.0 (CrossShell for UNIX)\n";
}
