#include "signal_parser.hpp"
#include "stop_options.hpp"

int StopOptions::Parse(int argc, char* argv[]) {
        if (argc < 2) {
            showHelp = true;
            return 1;
        }

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--json") { outputFormat = OutputFormat::Json; continue; }
            if (arg == "--csv") { outputFormat = OutputFormat::Csv; continue; }
            if (arg == "--table") { outputFormat = OutputFormat::Table; continue; }
            if (arg == "--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }

            if (arg == "--help" || arg == "-h") {
                showHelp = true;
                return 0;
            }

            if (arg == "--version") {
                showVersion = true;
                return 0;
            }

            if (arg == "--") {
                for (++i; i < argc; ++i) {
                    std::string pidArg = argv[i];
                    char* end = nullptr;
                    unsigned long parsed = std::strtoul(pidArg.c_str(), &end, 10);
                    if (end == nullptr || *end != '\0' || parsed == 0 || parsed > 0xFFFFFFFFUL) {
                        std::cerr << "stop: illegal process id: " << pidArg << "\n";
                        return 1;
                    }
                    pids.push_back(static_cast<DWORD>(parsed));
                }
                break;
            }

            if (arg == "-l" || arg == "-L" || arg == "--list") {
                showSignalList = true;
                return 0;
            }

            if (arg == "-s" || arg == "--signal") {
                if (i + 1 >= argc) {
                    std::cerr << "stop: missing signal argument\n";
                    PrintUsage();
                    return 1;
                }
                if (!SignalParser::Parse(argv[++i], signal)) {
                    std::cerr << "stop: unknown signal: " << argv[i] << "\n";
                    return 1;
                }
                continue;
            }

            if (arg.size() > 1 && arg[0] == '-') {
                std::cerr << "stop: unknown option -- " << arg << "\n";
                PrintUsage();
                return 1;
            }

            char* end = nullptr;
            unsigned long parsed = std::strtoul(arg.c_str(), &end, 10);
            if (end == nullptr || *end != '\0' || parsed == 0 || parsed > 0xFFFFFFFFUL) {
                std::cerr << "stop: illegal process id: " << arg << "\n";
                return 1;
            }
            pids.push_back(static_cast<DWORD>(parsed));
        }

        if (pids.empty()) {
            std::cerr << "stop: no process IDs supplied\n";
            PrintUsage();
            return 1;
        }

        return -1;
    }

void StopOptions::PrintUsage() const {
        std::cout << R"(stop(1)                 CrossShell for UNIX Reference Manual                  stop(1)

    NAME
        stop - pause or suspend running processes

    SYNOPSIS
        stop [OPTIONS] PID...

    DESCRIPTION
        stop pauses the execution of processes by suspending all associated
        threads (equivalent to SIGSTOP / NtSuspendProcess).

    OPTIONS
        -l, --list
            List supported signal names and actions.

        --json, --csv, --table
            Format suspension results as JSON, CSV, or table.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        stop 1234
            Suspend execution of process 1234.

    CrossShell for UNIX                                                   stop(1)
)";
    }

void StopOptions::PrintVersion() const {
        std::cout << "stop v2.1.0\n";
    }
