#include "ps_app.hpp"

void PsApplication::displayHelp() {
    std::wcout << LR"(ps(1)                   CrossShell for UNIX Reference Manual                       ps(1)

    NAME
        ps - report active Windows processes

    SYNOPSIS
        ps [QUALIFIERS] [OUTPUT-OPTIONS]

    DESCRIPTION
        Reports active processes using familiar UNIX-style qualifiers and selectable
        output columns. Structured output is written to standard output and is safe
        to pipe to jq, ConvertFrom-Csv, Select-String, or another command.

    OPTIONS
        -e, -A, -a
            Select all active processes.

        -f
            Use the full format: USER, PID, PPID, STIME, TIME, COMMAND.

        -l
            Use the long format: STAT, PRI, THREADS, PID, PPID, VSZ, MEM, TIME,
            COMMAND.

        -p, -q, --pid PIDS
            Filter by a comma-delimited PID list, such as 1042,4088.

        -u, --user USER
            Filter by process owner or account username.

        -o, --format COLUMNS
            Select a comma-delimited list of output columns.

        --output FORMAT
            Select table, csv, json, or xml output. The default is table.

        --table
            Shortcut for --output table.

        --json
            Shortcut for --output json.

        --xml
            Shortcut for --output xml.

        --csv
            Shortcut for --output csv.

    AVAILABLE COLUMNS
        user, owner
            Process owner or user account name.

        pid
            Process identifier.

        ppid
            Parent process identifier.

        mem, rss, pmem
            Resident memory or working set.

        vsz, vsize
            Virtual memory size or pagefile usage.

        cpu, time, cputime
            Total accumulated CPU execution time.

        threads, thcnt
            Number of active threads.

        pri, priority
            Base priority class.

        stat, state
            Execution status.

        stime, start
            Process creation start time.

        comm, cmd, command
            Executable process name.

    DEFAULT LAYOUT
        USER, PID, MEM, CPU, THREADS, COMMAND

    EXAMPLES
        ps
            Display processes using the default layout.

        ps -ef
            Display all active processes in full format.

        ps -u Alice --json
            Emit Alice's processes as JSON.

        ps -p 1204,4096 -o pid,user,mem,cpu,command
            Display selected columns for two process IDs.

        ps -o user,pid,threads,mem --csv
            Emit selected process data as CSV.

        ps --json | jq '.[].command'
            Extract command names from JSON output.

        ps --csv | ConvertFrom-Csv | Where-Object { $_.USER -match 'SYSTEM' }
            Filter CSV output in PowerShell.

    CrossShell for UNIX                                                          ps(1)
)";
}

int PsApplication::run(int argc, wchar_t* argv[]) {
    PsConfig config = CommandLineParser::parse(argc, argv);

    if (config.showHelp) {
        displayHelp();
        return 0;
    }
    if (config.parseError) {
        std::wcerr << L"ps: unsupported output format (expected table, csv, json, or xml)\n";
        return 2;
    }

    // 1. Gather active processes
    auto all = ProcessCollector::collect();
    std::vector<ProcessRecord> filtered;

    // 2. Filter records
    for (const auto& proc : all) {
        if (!config.pidFilter.empty() && config.pidFilter.find(proc.pid) == config.pidFilter.end()) {
            continue;
        }
        if (!config.userFilter.empty()) {
            std::wstring userLower = proc.user;
            std::wstring filterLower = config.userFilter;
            std::transform(userLower.begin(), userLower.end(), userLower.begin(), ::towlower);
            std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::towlower);
            if (userLower.find(filterLower) == std::wstring::npos) {
                continue;
            }
        }
        filtered.push_back(proc);
    }

    // 3. Resolve active column format
    std::vector<std::wstring> columns;
    if (!config.customColumns.empty()) {
        columns = config.customColumns;
    } else if (config.longFormat) {
        columns = { L"stat", L"pri", L"threads", L"pid", L"ppid", L"vsz", L"mem", L"cpu", L"command" };
    } else if (config.fullFormat) {
        columns = { L"user", L"pid", L"ppid", L"stime", L"cpu", L"command" };
    } else {
        // Default layout required: owner, PID, memory, CPU, threads, command
        columns = { L"user", L"pid", L"mem", L"cpu", L"threads", L"command" };
    }

    // 4. Dispatch Formatter Strategy
    std::unique_ptr<IOutputFormatter> formatter;
    switch (config.mode) {
        case OutputMode::CSV:  formatter = std::make_unique<CsvFormatter>(); break;
        case OutputMode::JSON: formatter = std::make_unique<JsonFormatter>(); break;
        case OutputMode::XML:  formatter = std::make_unique<XmlFormatter>(); break;
        case OutputMode::TABLE:
        default:               formatter = std::make_unique<TableFormatter>(); break;
    }

    formatter->render(filtered, columns);
    std::wcout.flush();
    return 0;
}
