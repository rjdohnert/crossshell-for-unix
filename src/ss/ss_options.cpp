#include "ss_options.hpp"

bool SsOptions::Parse(int argc, wchar_t* argv[]) {
        bool modeSpecified = false;
        showTcp = true;
        showUdp = true;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";

            if (arg == L"-h" || arg == L"--help") {
                showHelp = true;
                return true;
            }
            if (arg == L"--version" || arg == L"-V") {
                showVersion = true;
                return true;
            }
            if (arg == L"-n" || arg == L"--numeric") {
                continue;
            }
            if (arg == L"-l" || arg == L"--listening") {
                listeningOnly = true;
                continue;
            }
            if (arg == L"-t" || arg == L"--tcp") {
                if (!modeSpecified) {
                    showTcp = false;
                    showUdp = false;
                    modeSpecified = true;
                }
                showTcp = true;
                continue;
            }
            if (arg == L"-u" || arg == L"--udp") {
                if (!modeSpecified) {
                    showTcp = false;
                    showUdp = false;
                    modeSpecified = true;
                }
                showUdp = true;
                continue;
            }

            std::wcerr << L"ss: unknown option -- " << arg << L"\n";
            return false;
        }

        return true;
    }

void SsOptions::PrintUsage(const wchar_t* programName) const {
        std::wcout << LR"(ss(1)                   CrossShell for UNIX Reference Manual                    ss(1)

    NAME
        ss - dump network socket statistics and active endpoints

    SYNOPSIS
        ss [OPTIONS] [FILTER]

    DESCRIPTION
        ss is used to dump socket statistics. It allows showing information
        similar to netstat, displaying active TCP, UDP, and raw sockets.

    OPTIONS
        -t, --tcp
            Display TCP sockets.

        -u, --udp
            Display UDP sockets.

        -l, --listening
            Display only listening sockets.

        -a, --all
            Display both listening and non-listening sockets.

        -n, --numeric
            Do not try to resolve service names or hostnames.

        -p, --processes
            Show process using socket.

        --json, --csv, --table
            Format socket records as JSON, CSV, or table.

        --pipe COMMAND
            Send output directly to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        ss -t -a
            Display all TCP sockets.

        ss -l -n --json
            Display listening sockets numerically in JSON format.

    CrossShell for UNIX                                                     ss(1)
)";
    }

void SsOptions::PrintVersion() const {
        std::wcout << L"ss 1.0.0\n";
    }
