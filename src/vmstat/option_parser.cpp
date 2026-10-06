#include "option_parser.hpp"
#include "string_utils.hpp"
#include "vmstat_options.hpp"

void OptionParser::ShowHelp() {
        std::cout << R"(vmstat(1)               CrossShell for UNIX Reference Manual                 vmstat(1)

    NAME
        vmstat - report virtual memory statistics

    SYNOPSIS
        vmstat [OPTIONS] [INTERVAL [COUNT]]

    DESCRIPTION
        Report information about processes, memory, paging, block IO, traps,
        and cpu activity over time.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

        -s, --summary
            Display event and memory summary counters and exit.

        -H, --human
            Display memory metrics in human-readable units (B/K/M/G).

        -M, --megabytes
            Display memory metrics in Megabytes (MB).

        -K, --kilobytes
            Display memory metrics in Kilobytes (KB) (default).

        -n, --lines NUMBER
            Repeat header output every NUMBER lines (default: 20).

        --output MODE
            Output mode: table, csv, json (default: table).

        --csv
            Shortcut for --output csv.

        --json
            Shortcut for --output json.

        --layout MODE
            Layout mode: auto, compact, single-line.

        --compact
            Force compact layout.

        --single-line
            Disable split-row compact layout.

    EXAMPLES
        vmstat 1 5
            Report every 1 second, 5 times.

        vmstat -H 2
            Human readable output updated every 2 seconds.

        vmstat --csv 1 5
            CSV output for automation.

        vmstat -s
            Display system memory summary.

    CrossShell for UNIX                                                      vmstat(1)
)";
    }

void OptionParser::ShowVersion() {
        std::cout << "vmstat v1.0.0\nCopyright (C) 2026\n\n";
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], VmstatOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = StringUtils::WStrToStr(argv[i]);
            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                ShowHelp();
                exitEarly = true;
                return true;
            } else if (arg == "-v" || arg == "--version") {
                ShowVersion();
                exitEarly = true;
                return true;
            } else if (arg == "-s" || arg == "--summary") {
                opts.summaryMode = true;
            } else if (arg == "-H" || arg == "--human") {
                opts.mode = UnitMode::Human;
            } else if (arg == "-M" || arg == "--megabytes") {
                opts.mode = UnitMode::Megabytes;
            } else if (arg == "-K" || arg == "--kilobytes") {
                opts.mode = UnitMode::Kilobytes;
            } else if (arg == "--csv") {
                opts.outputMode = OutputMode::Csv;
            } else if (arg == "--json") {
                opts.outputMode = OutputMode::Json;
            } else if (arg == "--compact") {
                opts.layoutMode = LayoutMode::Compact;
            } else if (arg == "--single-line") {
                opts.layoutMode = LayoutMode::SingleLine;
            } else if (arg == "--output") {
                if (i + 1 >= argc) {
                    std::cerr << "vmstat: missing value for --output.\n";
                    return false;
                }
                std::string out = StringUtils::WStrToStr(argv[++i]);
                if (out == "table") opts.outputMode = OutputMode::Table;
                else if (out == "csv") opts.outputMode = OutputMode::Csv;
                else if (out == "json") opts.outputMode = OutputMode::Json;
                else {
                    std::cerr << "vmstat: invalid --output mode. Use table, csv, or json.\n";
                    return false;
                }
            } else if (arg == "--layout") {
                if (i + 1 >= argc) {
                    std::cerr << "vmstat: missing value for --layout.\n";
                    return false;
                }
                std::string lm = StringUtils::WStrToStr(argv[++i]);
                if (lm == "auto") opts.layoutMode = LayoutMode::Auto;
                else if (lm == "compact") opts.layoutMode = LayoutMode::Compact;
                else if (lm == "single-line") opts.layoutMode = LayoutMode::SingleLine;
                else {
                    std::cerr << "vmstat: invalid --layout mode. Use auto, compact, or single-line.\n";
                    return false;
                }
            } else if (arg == "-n" || arg == "--lines") {
                if (i + 1 < argc) {
                    std::string linesVal = StringUtils::WStrToStr(argv[++i]);
                    if (!StringUtils::TryParseInt(linesVal, 1, opts.headerInterval)) {
                        std::cerr << "vmstat: invalid value for -n/--lines. Expected integer >= 1.\n";
                        return false;
                    }
                } else {
                    std::cerr << "vmstat: missing value for -n/--lines.\n";
                    return false;
                }
            } else if (arg[0] != '-') {
                opts.positionalArgs.push_back(arg);
            } else {
                std::cerr << "vmstat: unknown option '" << arg << "'\nTry 'vmstat --help' for usage.\n";
                return false;
            }
        }

        if (!opts.positionalArgs.empty()) {
            if (!StringUtils::TryParseInt(opts.positionalArgs[0], 1, opts.interval)) {
                std::cerr << "vmstat: invalid interval '" << opts.positionalArgs[0] << "'. Expected integer >= 1.\n";
                return false;
            }
            if (opts.positionalArgs.size() > 1) {
                if (!StringUtils::TryParseInt(opts.positionalArgs[1], 1, opts.maxCount)) {
                    std::cerr << "vmstat: invalid count '" << opts.positionalArgs[1] << "'. Expected integer >= 1.\n";
                    return false;
                }
            }
            if (opts.positionalArgs.size() > 2) {
                std::cerr << "vmstat: too many positional arguments.\nTry 'vmstat --help' for usage.\n";
                return false;
            }
        }

        return true;
    }
