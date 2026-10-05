// ============================================================================
// 5. Configuration & Command Line Parser
// ============================================================================
enum class OutputType { Table, CSV, JSON, Pipe };
enum class SortColumn { Handles, PID, Name };

struct AppConfig {
    OutputType format = OutputType::Table;
    SortColumn sortBy = SortColumn::Handles;
    bool sortDescending = true;
    size_t topN = 0; // 0 = all
    std::wstring filterName = L"";
    DWORD filterPid = 0;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring pipeCommand = L"";
};

class CommandLineParser {
public:
    static AppConfig parse(int argc, wchar_t* argv[]) {
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
                if (s == L"handles") config.sortBy = SortColumn::Handles;
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
            }
        }
        return config;
    }

    static void printHelp() {
        std::wcout << LR"(handles(1)              CrossShell for UNIX Reference Manual              handles(1)

    NAME
        handles - inspect and query process handle counts across running tasks

    SYNOPSIS
        handles [OPTIONS]
        handles [-f FORMAT] [-s COLUMN] [--asc|--desc] [-n NAME] [-p PID] [-t COUNT]

    DESCRIPTION
        handles queries the NT kernel diagnostic system to enumerate active
        processes and report their open handle allocations. It provides
        filtering by process name or identifier, sorting by handle consumption,
        and structured output serialization.

    OPTIONS
        -f, --format FORMAT
            Output serialization format: table, csv, json, or pipe.
            Default is table.

        -s, --sort COLUMN
            Sort process list by COLUMN: 'handles' (default), 'pid', or 'name'.

        --asc
            Sort records in ascending order.

        --desc
            Sort records in descending order (default).

        -n, --name STRING
            Filter processes matching the specified name substring.

        -p, --pid PID
            Filter by exact Process Identifier (PID).

        -t, --top N
            Limit results to the top N processes.

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
        handles
            Display handle table for all running processes sorted by handle count.

        handles -t 10
            Display the top 10 processes consuming the most handles.

        handles -n svchost
            List all svchost processes and their handle allocations.

        handles -p 1234 --json
            Query handle statistics for PID 1234 formatted as JSON.

        handles -f csv --asc -s name
            Export all processes in CSV format sorted alphabetically by name.

    CrossShell for UNIX                                                 handles(1)
)";
    }

    static void printVersion() {
        std::wcout << L"handles (CrossShell) 5.0.0\n"
                   << L"Copyright (c) 2026 PC/OpenSystems LLC contributors. All rights reserved.\n";
    }
};

