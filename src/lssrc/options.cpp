#include "options.hpp"
#include <iostream>

bool LssrcOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"-a") {
            includeStopped = true;
            continue;
        }
        if (arg == L"-h" || arg == L"--help") {
            showHelp = true;
            return true;
        }
        if (arg == L"-v" || arg == L"-V" || arg == L"--version") {
            showVersion = true;
            return true;
        }
        if (arg == L"-s" && i + 1 < argc) {
            specificService = argv[++i] ? argv[i] : L"";
            continue;
        }
        if (arg == L"--table") { format = LssrcFormat::Table; continue; }
        if (arg == L"--csv") { format = LssrcFormat::Csv; continue; }
        if (arg == L"--json") { format = LssrcFormat::Json; continue; }
        if (arg == L"-") {
            std::wstring token;
            while (std::wcin >> token) filters.push_back(token);
            continue;
        }
        if (!arg.empty() && arg[0] != L'-') {
            filters.push_back(arg);
            continue;
        }
        std::wcerr << L"lssrc: unknown option '" << arg << L"'\n";
        return false;
    }
    return true;
}

void LssrcOptions::PrintUsage(const char* /*prog*/) const {
    std::cout << R"(lssrc(1)                 CrossShell for UNIX Reference Manual                 lssrc(1)

NAME
    lssrc - list Windows service status

SYNOPSIS
    lssrc [OPTIONS] [FILTER...]

DESCRIPTION
    Lists Windows services through the Service Control Manager. By default,
    active services are shown; filters match service names case-insensitively.

OPTIONS
    -a
        Include stopped services.

    -s NAME
        Select exactly one service by service name.

    --table
        Use aligned table output (default).

    --csv
        Emit CSV output.

    --json
        Emit a JSON array.

    -
        Read service filters from standard input.

    -h, --help
        Display this comprehensive reference manual and exit.

    -v, -V, --version
        Display version information and exit.

OUTPUT
    Table columns are Subsystem, PID, and Status. CSV fields are Service, PID,
    and Status; JSON objects contain service, pid, and status.

EXAMPLES
    lssrc
        List active services in table format.

    lssrc -a --json
        Include stopped services and emit JSON.

    lssrc -s Spooler
        Show the exact status of the Print Spooler service.

    echo update | lssrc - --csv
        Read a service-name filter from standard input.

EXIT STATUS
    0
        Help, version, or successful enumeration.
    1
        Invalid input, service-manager failure, or missing service.

CrossShell for UNIX                                                    lssrc(1)
)";
}

void LssrcOptions::PrintVersion() const {
    std::cout << "lssrc 1.0.0\n";
}
