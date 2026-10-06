#include "option_parser.hpp"
#include "output_formatter.hpp"
#include "stopsrc_options.hpp"

void OptionParser::PrintHelp() {
        std::wcout << LR"(stopsrc(1)              CrossShell for UNIX Reference Manual                  stopsrc(1)

    NAME
        stopsrc - stop a Windows service subsystem

    SYNOPSIS
        stopsrc -s SERVICE [-t SECONDS] [OPTIONS]

    DESCRIPTION
        Stops a running Windows service using AIX System Resource Controller (SRC)
        syntax. Requests service stop and monitors state transitions via the Windows
        Service Control Manager (SCM) until stopped or the timeout expires.

    OPTIONS
        -s SERVICE
            Specify the Windows service name or short identifier to stop.

        -t SECONDS
            Wait timeout in seconds for the service to stop (default: 30).

        --json
            Emit result telemetry in JSON format.

        --csv
            Emit result telemetry in CSV format.

        --table
            Emit result telemetry in tabular format.

        --pipe COMMAND
            Stream status directly into COMMAND.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version information and exit.

    EXAMPLES
        stopsrc -s wuauserv
            Stop the Windows Update service with default 30-second timeout.

        stopsrc -s Spooler -t 60
            Stop the Print Spooler service with a 60-second timeout.

    CrossShell for UNIX                                                     stopsrc(1)
    )";
    }

void OptionParser::PrintVersion() {
        std::cout << "stopsrc v1.0.0\n";
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], StopsrcOptions& opt) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                opt.help = true;
                return true;
            }
            if (arg == L"-v" || arg == L"--version") {
                opt.version = true;
                return true;
            }
            if (arg == L"--json") { opt.format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { opt.format = OutputFormat::Csv; continue; }
            if (arg == L"--table") { opt.format = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opt.pipe = argv[++i]; continue; }
            if (arg == L"-s" && i + 1 < argc) {
                opt.service = argv[++i];
                continue;
            }
            if (arg == L"-t" && i + 1 < argc) {
                wchar_t* end = nullptr;
                unsigned long s = wcstoul(argv[++i], &end, 10);
                if (end == argv[i] || *end != L'\0' || s == 0) {
                    std::wcerr << L"stopsrc: invalid timeout '" << argv[i] << L"'\n";
                    return false;
                }
                opt.timeoutMs = static_cast<DWORD>(s * 1000UL);
                continue;
            }
            std::wcerr << L"stopsrc: unknown option '" << arg << L"'\n";
            return false;
        }
        return true;
    }
