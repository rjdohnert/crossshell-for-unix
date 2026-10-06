#include "options.hpp"

bool PinkyOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--") {
            for (int j = i + 1; j < argc; ++j) {
                targets.push_back(argv[j]);
            }
            break;
        } else if (arg == L"-s" || arg == L"-q" || arg == L"--short") {
            forceShort = true;
        } else if (arg == L"-l" || arg == L"--long") {
            forceLong = true;
        } else if (arg == L"-p" || arg == L"--plan") {
            printPlan = false;
        } else if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            showHelp = true;
            return true;
        } else if (arg == L"--version" || arg == L"-V") {
            showVersion = true;
            return true;
        } else {
            targets.push_back(arg);
        }
    }
    return true;
}

void PinkyOptions::PrintUsage(const wchar_t* exe) const {
    (void)exe;
    std::wcout << LR"(pinky(1)                CrossShell for UNIX Reference Manual                 pinky(1)

    NAME
        pinky - lightweight finger user information query tool

    SYNOPSIS
        pinky [OPTIONS] [USER...]

    DESCRIPTION
        pinky displays information about current Windows terminal, SSH, and local
        user sessions.

    OPTIONS
        -l
            Produce long format output for the specified USERs.

        -s, -q
            Produce short format output (default).

        -p
            Omit the user's plan file / notes in long format.

        -h
            Omit the project file / profile line in long format.

        --json, --csv, --table
            Output user records as JSON, CSV, or table.

        --pipe COMMAND
            Stream results into COMMAND.

        --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        pinky
            List all active interactive users in short format.

        pinky -l Administrator
            Display detailed long profile for Administrator.

    CrossShell for UNIX                                                  pinky(1)
)";
}

void PinkyOptions::PrintVersion() const {
    std::wcout << L"pinky 1.0.0\n";
}
