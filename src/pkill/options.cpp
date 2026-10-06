#include "options.hpp"

int PkillOptions::Parse(int argc, wchar_t* argv[]) {
    bool afterDoubleDash = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";

        if (!afterDoubleDash && arg == L"--") {
            afterDoubleDash = true;
            continue;
        }

        if (!afterDoubleDash && (arg == L"-h" || arg == L"--help")) {
            showHelp = true;
            return 0;
        }

        if (!afterDoubleDash && arg == L"--version") {
            showVersion = true;
            return 0;
        }

        if (!afterDoubleDash && (arg == L"-i" || arg == L"--ignore-case")) {
            ignoreCase = true;
            continue;
        }

        if (!afterDoubleDash && (arg == L"-x" || arg == L"--exact")) {
            exact = true;
            continue;
        }

        if (!afterDoubleDash && (arg == L"-e" || arg == L"--echo")) {
            echo = true;
            continue;
        }

        if (!afterDoubleDash && !arg.empty() && arg[0] == L'-') {
            std::wcerr << L"pkill: unknown option -- " << arg << L"\n";
            return 2;
        }

        if (!pattern.empty()) {
            std::wcerr << L"pkill: only one pattern is supported\n";
            return 2;
        }
        pattern = arg;
    }

    if (pattern.empty()) {
        std::wcerr << L"pkill: missing pattern\n";
        PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"pkill");
        return 2;
    }

    return -1;
}

void PkillOptions::PrintUsage(const wchar_t* programName) const {
    (void)programName;
    std::wcout << LR"(pkill(1)                CrossShell for UNIX Reference Manual                 pkill(1)

    NAME
        pkill - look up or signal processes based on name and attributes

    SYNOPSIS
        pkill [OPTIONS] PATTERN

    DESCRIPTION
        pkill looks through the currently running processes and terminates or
        signals processes matching the specified PATTERN regular expression.

    OPTIONS
        -f, --full
            Match pattern against full command line instead of executable name.

        -i, --ignore-case
            Match case-insensitively.

        -e, --echo
            Display what process is being signaled/killed.

        -signal, --signal SIGNAL
            Send specified signal (e.g., 9, 15, KILL, TERM).

        --json, --csv, --table
            Format termination results as JSON, CSV, or table.

        --pipe COMMAND
            Send results through COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        pkill -i notepad
            Kill all processes whose name contains notepad.

        pkill -f -e "node server.js"
            Kill node server matching full command line.

    CrossShell for UNIX                                                  pkill(1)
)";
}

void PkillOptions::PrintVersion() const {
    std::wcout << L"pkill v1.0.0\n";
}
