#include "option_parser.hpp"
#include "startsrc_options.hpp"

void OptionParser::PrintHelp() {
        std::wcout << LR"(startsrc(1)             CrossShell for UNIX Reference Manual                 startsrc(1)

    NAME
        startsrc - start a Windows service subsystem

    SYNOPSIS
        startsrc -s SERVICE [-t SECONDS] [OPTIONS]

    DESCRIPTION
        Starts a Windows service using AIX System Resource Controller (SRC)
        syntax. Queries and monitors service state transitions via the Windows
        Service Control Manager (SCM) until running or the timeout expires.

    OPTIONS
        -s SERVICE
            Specify the Windows service name or short identifier to start.

        -t SECONDS
            Wait timeout in seconds for the service to start (default: 30).

        -h, --help
            Display this reference manual.

        -v, --version
            Display version information and exit.

    EXAMPLES
        startsrc -s wuauserv
            Start the Windows Update service with default 30-second timeout.

        startsrc -s Spooler -t 60
            Start the Print Spooler service with a 60-second timeout.

    CrossShell for UNIX                                                    startsrc(1)
    )";
    }

void OptionParser::PrintVersion() {
        std::cout << "startsrc v1.0.0\n";
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], StartsrcOptions& opt) const {
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
            if (arg == L"-s" && i + 1 < argc) {
                opt.service = argv[++i];
                continue;
            }
            if (arg == L"-t" && i + 1 < argc) {
                wchar_t* end = nullptr;
                unsigned long s = wcstoul(argv[++i], &end, 10);
                if (end == argv[i] || *end != L'\0' || s == 0) {
                    std::wcerr << L"startsrc: invalid timeout '" << argv[i] << L"'\n";
                    return false;
                }
                opt.timeoutMs = static_cast<DWORD>(s * 1000UL);
                continue;
            }
            std::wcerr << L"startsrc: unknown option '" << arg << L"'\n";
            return false;
        }
        return true;
    }
