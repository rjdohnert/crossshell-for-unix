#include "command_line_parser.hpp"
#include "threads_options.hpp"

AppConfig CommandLineParser::parse(int argc, wchar_t* argv[]) {
        AppConfig config;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                config.showHelp = true;
                return config;
            } else if (arg == L"-V" || arg == L"--version") {
                config.showVersion = true;
                return config;
            } else if (arg == L"--json" || arg == L"-j") {
                config.format = OutputType::JSON;
            } else if (arg == L"--csv") {
                config.format = OutputType::CSV;
            } else if (arg == L"--table") {
                config.format = OutputType::Table;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                config.pipeCommand = argv[++i];
            } else if ((arg == L"-f" || arg == L"--format" || arg == L"--output") && i + 1 < argc) {
                std::wstring fmt = argv[++i];
                if (fmt == L"table") config.format = OutputType::Table;
                else if (fmt == L"csv") config.format = OutputType::CSV;
                else if (fmt == L"json") config.format = OutputType::JSON;
                else if (fmt == L"pipe") config.format = OutputType::Pipe;
            } else if ((arg == L"-s" || arg == L"--sort") && i + 1 < argc) {
                std::wstring s = argv[++i];
                if (s == L"threads") config.sortBy = SortColumn::Threads;
                else if (s == L"pid") config.sortBy = SortColumn::PID;
                else if (s == L"name") config.sortBy = SortColumn::Name;
            } else if (arg == L"--asc") {
                config.sortDescending = false;
            } else if (arg == L"--desc") {
                config.sortDescending = true;
            } else if ((arg == L"-n" || arg == L"--name") && i + 1 < argc) {
                config.filterName = argv[++i];
            } else if ((arg == L"-p" || arg == L"--pid") && i + 1 < argc) {
                config.filterPid = std::stoul(argv[++i]);
            } else if ((arg == L"-t" || arg == L"--top") && i + 1 < argc) {
                config.topN = std::stoul(argv[++i]);
            } else if ((arg == L"-m" || arg == L"--min-threads") && i + 1 < argc) {
                config.minThreads = std::stoul(argv[++i]);
            }
        }
        return config;
    }

void CommandLineParser::printHelp() {
        std::wcout << LR"(threads(1)              CrossShell for UNIX Reference Manual              threads(1)

    NAME
        threads - inspect and query thread counts across active processes

    SYNOPSIS
        threads [OPTIONS]
        threads [-f FORMAT] [-s COLUMN] [--asc|--desc] [-n NAME] [-p PID] [-m MIN] [-t COUNT]

    DESCRIPTION
        threads captures a snapshot of the active Windows process tree and
        enumerates thread allocations per process. It supports filtering by
        process identifier, process image name, or minimum thread threshold,
        sorting by thread count, and exporting structured data.

    OPTIONS
        -f, --format FORMAT
            Output serialization format: table, csv, json, or pipe.
            Default is table.

        -s, --sort COLUMN
            Sort process list by COLUMN: 'threads' (default), 'pid', or 'name'.

        --asc
            Sort records in ascending order.

        --desc
            Sort records in descending order (default).

        -n, --name STRING
            Filter processes matching the specified name substring.

        -p, --pid PID
            Filter by exact Process Identifier (PID).

        -m, --min-threads COUNT
            Filter out processes with fewer than COUNT threads.

        -t, --top N
            Limit output to the top N processes.

        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        threads
            Display thread counts for all active processes sorted descending.

        threads -t 10
            Display the top 10 processes with the highest thread counts.

        threads -n chrome --json
            Query thread counts for all 'chrome' processes formatted as JSON.

        threads -m 50 -f csv
            Export all processes with at least 50 threads in CSV format.

        threads -s name --asc
            List processes and their thread counts sorted alphabetically.

    CrossShell for UNIX                                                 threads(1)
)";
    }

void CommandLineParser::printVersion() {
        std::wcout << L"threads (CrossShell) 5.0.0\n"
                   << L"Copyright (c) 2026 PC/OpenSystems LLC contributors. All rights reserved.\n";
    }
