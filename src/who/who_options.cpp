#include "who_options.hpp"

bool WhoOptions::Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--") {
                break;
            } else if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            } else if (arg == L"--version" || arg == L"-V") {
                showVersion = true;
                return true;
            } else if (arg == L"-H" || arg == L"--heading") {
                optHeader = true;
            } else if (arg == L"-q" || arg == L"--count") {
                optCount = true;
            } else if (arg == L"-b" || arg == L"--boot") {
                optBoot = true;
            } else if (arg == L"-a" || arg == L"--all") {
                optAll = true;
            } else if (arg == L"-u" || arg == L"--login") {
                optLogin = true;
            } else if (arg == L"am" && i + 1 < argc && (_wcsicmp(argv[i + 1], L"i") == 0)) {
                optAmI = true;
                break;
            } else if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"who: invalid option '" << arg << L"'\n";
                return false;
            }
        }
        return true;
    }

void WhoOptions::PrintUsage(const wchar_t* progName) const {
        std::wcout << LR"(who(1)                  CrossShell for UNIX Reference Manual                   who(1)

    NAME
        who - show who is logged on to the Windows system

    SYNOPSIS
        who [OPTIONS] [am i]

    DESCRIPTION
        who prints information about users who are currently logged on to local
        or Remote Desktop / terminal sessions.

    OPTIONS
        -a, --all
            Same as -b -d --login -p -r -t -T -u.

        -b, --boot
            Time of last system boot.

        -H, --heading
            Print line of column headings.

        -q, --count
            All login names and number of users logged on.

        -u, --login
            List users logged in.

        am i, am I
            Print information about the current terminal session only.

        --json, --csv, --table
            Output user session records as JSON, CSV, or table.

        --pipe COMMAND
            Stream output to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        who -H
            List all logged on users with table headers.

        who -b
            Display last system boot time.

    CrossShell for UNIX                                                    who(1)
)";
    }

void WhoOptions::PrintVersion() const {
        std::wcout << L"who 1.0.0\n";
    }
