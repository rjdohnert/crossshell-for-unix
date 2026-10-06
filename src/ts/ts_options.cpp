#include "ts_options.hpp"

void TsOptions::printVersion() {
        std::cout << "ts version " << VERSION << "\n"
                  << "A high-performance timestamping pipeline utility for Windows.\n";
    }

void TsOptions::printHelp(const char* exeName) {
        std::cout <<
R"(NAME
    ts - timestamp standard input lines for Windows

SYNOPSIS
    )" << exeName << R"( [OPTIONS] [FORMAT]
    <command> | )" << exeName << R"( [OPTIONS] [FORMAT]

DESCRIPTION
    ts prepends a timestamp to each line received from standard input (stdin)
    and writes the result to standard output (stdout).

OPTIONS
    -i, --incremental
        Prepend the elapsed time since the previous line (delta time).
    -s, --since-start
        Prepend the elapsed time since the program was launched.
    -u, --utc
        Format calendar timestamps using Universal Coordinated Time (UTC).
    -z, --iso
        Format calendar timestamps in strict ISO-8601 extended format.
    -m, --millis
        Include milliseconds (3 fractional digits).
    -u, --micros
        Include microseconds (6 fractional digits).
    -n, --nanos
        Include nanoseconds (9 fractional digits).
    --json, --csv, --table
        Format transformed output records.
    --pipe COMMAND
        Send formatted output through COMMAND.
    -h, --help
        Display this help message.
    -V, --version
        Display version and author information.

EXAMPLES
    ping 127.0.0.1 | ts
    build.bat | ts -s
    stream_sensor | ts -i -u "%.6s"
)";
    }

bool TsOptions::parse(int argc, char* argv[], TsOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-i" || arg == "--incremental") {
                opts.mode = TsMode::Incremental;
            } else if (arg == "-s" || arg == "--since-start") {
                opts.mode = TsMode::ElapsedSinceStart;
            } else if (arg == "-u" || arg == "--utc") {
                opts.useUtc = true;
            } else if (arg == "-z" || arg == "--iso") {
                opts.isIso = true;
            } else if (arg == "-m" || arg == "--millis") {
                opts.subsecondPrecision = 3;
            } else if (arg == "--micros") {
                opts.subsecondPrecision = 6;
            } else if (arg == "-n" || arg == "--nanos") {
                opts.subsecondPrecision = 9;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (!arg.empty() && arg[0] != '-') {
                opts.customFormat = arg;
            } else if (arg.length() > 1 && arg[0] == '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    switch (c) {
                        case 'i': opts.mode = TsMode::Incremental; break;
                        case 's': opts.mode = TsMode::ElapsedSinceStart; break;
                        case 'u': opts.useUtc = true; break;
                        case 'z': opts.isIso = true; break;
                        case 'm': opts.subsecondPrecision = 3; break;
                        case 'n': opts.subsecondPrecision = 9; break;
                        default:
                            std::cerr << "ts: unrecognized option -- '" << c << "'\n";
                            return false;
                    }
                }
            }
        }
        return true;
    }
