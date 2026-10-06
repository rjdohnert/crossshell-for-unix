#include "options.hpp"

void PthctlOptions::ShowVersion() {
    std::wcout << PROGRAM_NAME << L" version " << VERSION << L"\n"
              << COPYRIGHT << L"\n"
              << L"License: BSD-3 Clause License\n"
              << L"Written by Roberto J Dohnert.\n";
}

void PthctlOptions::ShowHelp() {
    std::wcout << LR"(pthctl(1)               CrossShell for UNIX Reference Manual                 pthctl(1)

    NAME
        pthctl - query, modify, and deduplicate Windows PATH environment variables

    SYNOPSIS
        pthctl COMMAND [OPTIONS] [PATH]

    DESCRIPTION
        Manages user and system-level PATH environment variables directly in the
        Windows Registry. Supports inspecting, appending, prepending, verifying,
        and deduplicating PATH directories with real-time environment broadcasts
        and structured reporting.

    COMMANDS
        list, ls
            Display current PATH entries with indices and existence status.

        add PATH
            Append or prepend a directory entry to the PATH variable.

        remove, rm PATH
            Remove a specified directory entry from the PATH variable.

        check PATH
            Verify whether a directory entry is present in the PATH variable.

        clean
            Remove duplicate and non-existent directory entries from PATH.

    OPTIONS
        -u, --user
            Target the current User environment PATH variable (default).

        -s, --system
            Target the machine-wide System PATH variable (requires Admin).

        -p, --prepend
            Insert new path entries at the beginning of PATH rather than appending.

        -d, --dry-run
            Simulate modifications without committing changes to the registry.

        --json, --csv, --table
            Format output as JSON objects, CSV records, or an aligned table.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        pthctl list --user
            List all entries in the user PATH variable.

        pthctl list --system --table
            Display system PATH entries in an aligned tabular grid.

        pthctl add C:\Tools\bin --user
            Append C:\Tools\bin to the user PATH variable.

        pthctl add C:\Tools\bin --system --prepend
            Prepend C:\Tools\bin to the system-wide PATH variable.

        pthctl remove C:\OldTools\bin --user
            Remove an obsolete path entry from the user PATH.

        pthctl clean --user --dry-run
            Preview deduplication and cleanup of the user PATH.

        pthctl list --json
            Export PATH entries as structured JSON.

    CrossShell for UNIX                                                     pthctl(1)
)";
}

bool PthctlOptions::Parse(int argc, wchar_t* argv[], PthctlParsedArgs& parsed) {
    if (argc < 2) {
        ShowHelp();
        parsed.shouldExit = true;
        parsed.exitCode = 0;
        return true;
    }

    parsed.command = argv[1];

    if (parsed.command == L"-h" || parsed.command == L"--help") {
        ShowHelp();
        parsed.shouldExit = true;
        parsed.exitCode = 0;
        return true;
    }

    if (parsed.command == L"-v" || parsed.command == L"--version") {
        ShowVersion();
        parsed.shouldExit = true;
        parsed.exitCode = 0;
        return true;
    }

    bool prependSpecified = false;
    bool dryRunSpecified = false;
    bool userScopeSpecified = false;
    bool systemScopeSpecified = false;
    bool extraPositionalArg = false;

    // CLI Arguments Parsing
    for (int i = 2; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-s" || arg == L"--system") {
            if (userScopeSpecified) {
                std::wcerr << L"pthctl: error: '--user' and '--system' cannot be used together.\n"
                          << L"Try 'pthctl --help' for usage.\n";
                return false;
            }
            systemScopeSpecified = true;
            parsed.scope = Scope::System;
        } else if (arg == L"-u" || arg == L"--user") {
            if (systemScopeSpecified) {
                std::wcerr << L"pthctl: error: '--user' and '--system' cannot be used together.\n"
                          << L"Try 'pthctl --help' for usage.\n";
                return false;
            }
            userScopeSpecified = true;
            parsed.scope = Scope::User;
        } else if (arg == L"-p" || arg == L"--prepend") {
            parsed.prepend = true;
            prependSpecified = true;
        } else if (arg == L"-d" || arg == L"--dry-run") {
            parsed.dryRun = true;
            dryRunSpecified = true;
        } else if (arg == L"--json" || arg == L"--csv" || arg == L"--table") {
            parsed.outputFormat = arg == L"--json" ? OutputFormat::Json : (arg == L"--csv" ? OutputFormat::Csv : OutputFormat::Table);
        } else if (arg == L"--pipe" && i + 1 < argc) {
            parsed.pipeCommand = argv[++i];
        } else if (arg == L"-h" || arg == L"--help") {
            ShowHelp();
            parsed.shouldExit = true;
            parsed.exitCode = 0;
            return true;
        } else if (arg == L"-v" || arg == L"--version") {
            ShowVersion();
            parsed.shouldExit = true;
            parsed.exitCode = 0;
            return true;
        } else if (!arg.empty() && arg[0] == L'-') {
            std::wcerr << L"pthctl: unrecognized option '" << arg << L"'\n"
                      << L"Try 'pthctl --help' for usage.\n";
            return false;
        } else {
            if (parsed.pathArg.empty()) {
                parsed.pathArg = arg;
            } else {
                extraPositionalArg = true;
            }
        }
    }

    if (extraPositionalArg) {
        std::wcerr << L"pthctl: error: too many positional arguments.\n"
                  << L"Try 'pthctl --help' for usage.\n";
        return false;
    }

    if (prependSpecified && parsed.command != L"add") {
        std::wcerr << L"pthctl: error: '--prepend' is only valid with the 'add' command.\n"
                  << L"Try 'pthctl --help' for usage.\n";
        return false;
    }

    if (dryRunSpecified && parsed.command != L"add" && parsed.command != L"remove" && parsed.command != L"rm" && parsed.command != L"clean") {
        std::wcerr << L"pthctl: error: '--dry-run' is only valid with 'add', 'remove', and 'clean'.\n"
                  << L"Try 'pthctl --help' for usage.\n";
        return false;
    }

    return true;
}
