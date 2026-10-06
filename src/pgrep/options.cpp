#include "options.hpp"

void OptionParser::PrintUsage(const wchar_t* programName) {
    (void)programName;
    std::wcout << LR"HELP(pgrep(1)                 CrossShell for UNIX Reference Manual                   pgrep(1)

    NAME
        pgrep - find running processes by executable name

    SYNOPSIS
        pgrep [OPTIONS] PATTERN

    DESCRIPTION
        Matches running processes by executable name using substring matching by
        default. Process enumeration uses the Windows ToolHelp API.

    OPTIONS
        -i, --ignore-case   Ignore case distinctions.
        -x, --exact         Require an exact executable-name match.
        -l, --list-name     Print PID and process name.
        -c, --count         Print only the match count.
        -h, --help          Display this comprehensive reference manual and exit.
        --version           Display version information and exit.
        --                  End options; the remaining argument is PATTERN.

    EXAMPLES
        pgrep chrome
            Print PIDs for processes whose names contain chrome.
        pgrep -l -i notepad
            Print matching PIDs and names case-insensitively.
        pgrep -c --exact powershell.exe
            Print the count of exact matches.

    EXIT STATUS
        0          Help, version, or at least one matching process.
        1          No matching processes.
        2          Invalid arguments or process snapshot failure.

    CrossShell for UNIX                                                       pgrep(1)
)HELP";
}

void OptionParser::PrintVersion() {
    std::wcout << L"pgrep v1.0.0\n";
}

bool OptionParser::Parse(int argc, wchar_t* argv[], PgrepOptions& options) const {
    bool afterDoubleDash = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";

        if (!afterDoubleDash && arg == L"--") {
            afterDoubleDash = true;
            continue;
        }

        if (!afterDoubleDash && (arg == L"-h" || arg == L"--help")) {
            options.showHelp = true;
            return true;
        }

        if (!afterDoubleDash && arg == L"--version") {
            options.showVersion = true;
            return true;
        }

        if (!afterDoubleDash && (arg == L"-i" || arg == L"--ignore-case")) {
            options.ignoreCase = true;
            continue;
        }

        if (!afterDoubleDash && (arg == L"-x" || arg == L"--exact")) {
            options.exact = true;
            continue;
        }

        if (!afterDoubleDash && (arg == L"-l" || arg == L"--list-name")) {
            options.listName = true;
            continue;
        }

        if (!afterDoubleDash && (arg == L"-c" || arg == L"--count")) {
            options.countOnly = true;
            continue;
        }

        if (!afterDoubleDash && !arg.empty() && arg[0] == L'-') {
            std::wcerr << L"pgrep: unknown option -- " << arg << L"\n";
            return false;
        }

        if (!options.pattern.empty()) {
            std::wcerr << L"pgrep: only one pattern is supported\n";
            return false;
        }
        options.pattern = arg;
    }

    if (options.pattern.empty()) {
        std::wcerr << L"pgrep: missing pattern\n";
        return false;
    }

    return true;
}
