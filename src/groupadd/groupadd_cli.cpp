#include "groupadd_cli.hpp"

#include <iostream>
#include <utility>

namespace {
bool ReadValue(int& index, int argc, wchar_t* argv[], const wchar_t* option,
               std::wstring& value, std::wstring& error) {
    if (index + 1 >= argc || argv[index + 1] == nullptr) {
        error = std::wstring(option) + L" requires an argument";
        return false;
    }
    value = argv[++index];
    return true;
}
}

bool CommandLineParser::Parse(int argc, wchar_t* argv[],
                              CommandOptions& options,
                              std::wstring& error) {
    options = CommandOptions{};
    error.clear();
    bool positionalOnly = false;
    bool hasGroupName = false;

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
            (argument == L"-r" || argument == L"--system")) {
            options.systemGroup = true;
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
        if (!positionalOnly &&
            (argument == L"-c" || argument == L"--comment")) {
            if (!ReadValue(i, argc, argv, argument.c_str(),
                           options.comment, error)) {
                return false;
            }
            continue;
        }
        if (!positionalOnly && argument == L"-") {
            if (hasGroupName || options.readFromStdin) {
                error = L"standard input cannot be combined with a group operand";
                return false;
            }
            options.readFromStdin = true;
            continue;
        }
        if (!positionalOnly && !argument.empty() && argument[0] == L'-') {
            error = L"unrecognized option";
            return false;
        }

        if (options.readFromStdin) {
            error = L"standard input cannot be combined with a group operand";
            return false;
        }
        if (hasGroupName) {
            error = L"too many group operands";
            return false;
        }
        options.groupName = argument;
        hasGroupName = true;
    }
    return true;
}

void CommandLineParser::PrintHelp() {
    std::wcout << LR"(groupadd(1)             CrossShell for UNIX Reference Manual              groupadd(1)

NAME
    groupadd - create a local Windows group

SYNOPSIS
    groupadd [OPTIONS] GROUP
    groupadd [OPTIONS] -
    type GROUPS.txt | groupadd [OPTIONS]
    Get-Content GROUPS.txt | groupadd [OPTIONS]

OPTIONS
    -c, --comment COMMENT
        Set the group's description. For input records, this is the default
        comment unless a delimited record supplies one.

    -f, --force
        Succeed if the local group already exists. This does not update it.

    -r, --system
        Accepted for compatibility. Windows local groups have no system-group
        designation; this option does not change the created group.

    -q, --quiet
        Suppress progress and success messages. Errors are still reported.

    -v, --verbose
        Display progress and the result of each operation.

    -h, --help, /?, -?
        Display this help.

    -V, --version
        Display version information.

    -
        Read UTF-8 group records from standard input.

INPUT RECORDS
    Each nonblank line contains one group name, optionally followed by a comma
    or tab and a description. Quoted CSV fields use doubled quotes. Lines whose
    first non-whitespace character is # are comments. A UTF-8 BOM is accepted.

EXIT STATUS
    0    All requested operations succeeded.
    2    Invalid options, operands, or input records.
    4    Windows account-management or input/output failure.
    9    A group already exists and -f was not specified.

EXAMPLES
    groupadd Developers
        Create a local group.

    groupadd -c "Core Engineering Team" Engineers
        Create a local group with a description.

    groupadd -f Developers
        Succeed if Developers already exists.

    Get-Content .\groups.csv | groupadd -v
        Create one group per UTF-8 input record.

CrossShell for UNIX                                                       groupadd(1)
)";
}

void CommandLineParser::PrintVersion() {
    std::wcout << L"groupadd 3.0.0 (CrossShell for UNIX)\n";
}
