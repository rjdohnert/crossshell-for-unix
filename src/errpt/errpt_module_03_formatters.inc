void print_entries(const std::vector<EventEntry>& entries, Options::OutputFormat format) {
    if (format == Options::OutputFormat::Json) {
        std::cout << "[\n";
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            std::cout << "  {\"timestamp\":\"" << json_escape(entry.timestamp)
                      << "\",\"severity\":\"" << json_escape(entry.severity)
                      << "\",\"event_id\":" << entry.eventId
                      << ",\"source\":\"" << json_escape(entry.source)
                      << "\",\"summary\":\"" << json_escape(entry.summary) << "\"}"
                      << (i + 1 < entries.size() ? "," : "") << "\n";
        }
        std::cout << "]\n";
        return;
    }

    if (format == Options::OutputFormat::Csv) {
        std::cout << "timestamp,severity,event_id,source,summary\n";
        for (const auto& entry : entries) {
            std::cout << csv_escape(entry.timestamp) << ',' << csv_escape(entry.severity) << ','
                      << entry.eventId << ',' << csv_escape(entry.source) << ','
                      << csv_escape(entry.summary) << '\n';
        }
        return;
    }

    std::cout << std::left
              << std::setw(20) << "Timestamp"
              << std::setw(11) << "Severity"
              << std::setw(8) << "EventID"
              << std::setw(24) << "Source"
              << "Summary\n";
    for (const auto& entry : entries) {
        std::cout << std::setw(20) << entry.timestamp
                  << std::setw(11) << entry.severity
                  << std::setw(8) << entry.eventId
                  << std::setw(24) << entry.source.substr(0, 23)
                  << entry.summary << "\n";
    }
}

void print_help() {
    std::cout << R"(errpt(1)                 CrossShell for UNIX Reference Manual                    errpt(1)

NAME
    errpt - display Windows Event Log entries

SYNOPSIS
    errpt [OPTIONS]

DESCRIPTION
    Reads system, application, or security event logs and presents recent
    records as a table or structured data for scripts and pipelines.

OPTIONS
    -l, --log NAME        Select system, application, or security log.
    -n, --lines COUNT     Limit the number of entries (default: 50).
    -a, --all             Print all available entries.
    --output MODE         Select table, json, or csv output.
    --table               Use table output.
    --json                Emit a JSON array.
    --csv                 Emit CSV.
    --pipe COMMAND        Send formatted output through COMMAND.
    -h, --help            Display this comprehensive reference manual.
    -v, --version         Display version information and exit.

EXAMPLES
    errpt --json | jq '.[].summary'
    errpt --csv > system-events.csv
    errpt -l application --output table | findstr /i error

EXIT STATUS
    0          Help, version, or successful event-log query.
    1          Invalid options, unsupported log, or event-log failure.

CrossShell for UNIX                                                        errpt(1)
)";
}

void print_version() {
    std::cout << "errpt v1.0.0\n";
}
