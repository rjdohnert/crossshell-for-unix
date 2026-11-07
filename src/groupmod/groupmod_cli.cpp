#include "groupmod_cli.hpp"

#include <iostream>
#include <utility>

namespace {
bool ReadValue(int& index, int argc, wchar_t* argv[], const wchar_t* option,
               std::wstring& value, std::wstring& error) {
    if (index + 1 >= argc || argv[index + 1] == nullptr ||
        argv[index + 1][0] == L'-') {
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
            (argument == L"-n" || argument == L"--new-name")) {
            std::wstring value;
            if (!ReadValue(i, argc, argv, argument.c_str(), value, error)) {
                return false;
            }
            if (value.empty()) {
                error = L"--new-name requires a nonempty value";
                return false;
            }
            options.newName = std::move(value);
            continue;
        }
        if (!positionalOnly &&
            (argument == L"-c" || argument == L"--comment")) {
            std::wstring value;
            if (!ReadValue(i, argc, argv, argument.c_str(), value, error)) {
                return false;
            }
            options.comment = std::move(value);
            continue;
        }
        if (!positionalOnly &&
            argument.compare(0, 11, L"--new-name=") == 0) {
            const std::wstring value = argument.substr(11);
            if (value.empty()) {
                error = L"--new-name requires a nonempty value";
                return false;
            }
            options.newName = value;
            continue;
        }
        if (!positionalOnly &&
            argument.compare(0, 10, L"--comment=") == 0) {
            options.comment = argument.substr(10);
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
        if (!positionalOnly && argument[0] == L'-') {
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
    std::wcout << LR"(groupmod(1)             CrossShell for UNIX Reference Manual              groupmod(1)

NAME
    groupmod - modify a local Windows group

SYNOPSIS
    groupmod [OPTIONS] GROUP
    groupmod [OPTIONS] -
    type MODS.txt | groupmod [OPTIONS]
    Get-Content MODS.txt | groupmod [OPTIONS]

DESCRIPTION
    Renames a local group and/or updates its description through the Windows
    NetAPI. This command does not modify Active Directory groups or assign GIDs.

OPTIONS
    -n, --new-name NEW_GROUP
        Rename GROUP to NEW_GROUP.

    -c, --comment COMMENT
        Set or clear the group's description.

    -f, --force
        Succeed if the source group is absent. Does not suppress a name
        collision or bypass permissions and built-in group protection.

    -q, --quiet
        Suppress progress and success messages. Errors are still reported.

    -v, --verbose
        Display progress and the result of each operation.

    -h, --help, /?, -?
        Display this help.

    -V, --version
        Display version information.

    --
        Treat the remaining argument as a group name, even if it begins with -.

    -
        Read UTF-8 modification records from standard input.

INPUT RECORDS
    A record contains GROUP, optionally followed by comma- or tab-delimited
    NEW_GROUP and COMMENT fields. Quoted CSV fields use doubled quotes. With
    one field, command-line -n/-c defaults apply. An empty NEW_GROUP field
    means do not rename; an empty COMMENT field clears the description.
    Blank lines and lines whose first non-whitespace character is # are
    comments. A UTF-8 BOM is accepted. Records are limited to 64 KiB.

EXIT STATUS
    0    All requested modifications succeeded.
    2    Invalid options, operands, or input records.
    4    Other Windows account-management or input/output failure.
    6    Source group does not exist and -f was not specified.
    8    A built-in or protected group cannot be modified.
    9    The requested new group name is already in use.
    10   Access denied.

EXAMPLES
    groupmod -n SeniorEngineers Developers
        Rename a local group.

    groupmod -c "Core Engineering Team" DevOps
        Update a group description.

    groupmod -n SecOps -c "Security Operations" Security
        Rename a group and update its description in one operation.

    Get-Content .\group_updates.csv | groupmod -v
        Apply one UTF-8 modification record per line.

CrossShell for UNIX                                                       groupmod(1)
)";
}

void CommandLineParser::PrintVersion() {
    std::wcout << L"groupmod 3.0.0 (CrossShell for UNIX)\n";
}
