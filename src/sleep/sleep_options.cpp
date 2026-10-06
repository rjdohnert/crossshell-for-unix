#include "sleep_options.hpp"

void SleepOptionsParser::printHelp() {
    std::wcout << LR"(sleep(1)                CrossShell for UNIX Reference Manual                 sleep(1)

    NAME
        sleep - suspend execution for an interval of time

    SYNOPSIS
        sleep NUMBER[SUFFIX]...
        sleep [OPTIONS]

    DESCRIPTION
        sleep pauses process execution for the specified duration or the sum
        of all specified intervals. Each interval argument consists of a
        positive floating-point or integer number followed by an optional
        unit suffix. If no suffix is specified, seconds ('s') is assumed.

    QUALIFIERS AND UNITS
        s, sec, secs
            Seconds (default multiplier 1.0).

        m, min, mins
            Minutes (multiplier 60.0 seconds).

        h, hr, hrs, hours
            Hours (multiplier 3600.0 seconds).

        d, day, days
            Days (multiplier 86400.0 seconds).

        ms, msec, millis
            Milliseconds (multiplier 0.001 seconds).

        us, usec, micros
            Microseconds (multiplier 0.000001 seconds).

    OPTIONS
        -s, --seconds SECONDS
            Specify delay in seconds.

        -m, --minutes MINUTES
            Specify delay in minutes.

        -h, --hours HOURS
            Specify delay in hours.

        -d, --days DAYS
            Specify delay in days.

        --ms, --millis MILLIS
            Specify delay in milliseconds.

        -q, --quiet
            Suppress informational or warning messages.

        -v, --verbose
            Display timer start and countdown telemetry.

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
        sleep 5
            Pause execution for 5 seconds.

        sleep 1.5s 250ms
            Pause for 1.75 seconds total.

        sleep 10m
            Pause for 10 minutes.

        sleep 2h 30m
            Pause for 2.5 hours.

        sleep 3s --json
            Pause for 3 seconds and emit completion telemetry as JSON.

    CrossShell for UNIX                                                     sleep(1)
)";
}

void SleepOptionsParser::printVersion() {
    std::wcout << L"sleep (CrossShell) 5.0.0\n"
               << L"Copyright (c) 2026 Roberto J Dohnert. All rights reserved.\n";
}

bool SleepOptionsParser::parse(int argc, wchar_t* argv[], SleepOptions& options) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
            options.show_help = true;
            continue;
        }
        if (arg == L"-V" || arg == L"-v" || arg == L"--version") {
            if (arg == L"-v" && argc > 2 && (i + 1 < argc || options.raw_delays.size() > 0 || options.has_explicit)) {
                options.verbose = true;
                continue;
            }
            options.show_version = true;
            continue;
        }
        if (arg == L"--verbose") {
            options.verbose = true;
            continue;
        }
        if (arg == L"-q" || arg == L"--quiet") {
            options.quiet = true;
            continue;
        }
        if (arg == L"--json" || arg == L"-j") {
            options.format = OutputFormat::Json;
            continue;
        }
        if (arg == L"--csv") {
            options.format = OutputFormat::Csv;
            continue;
        }
        if (arg == L"--tsv") {
            options.format = OutputFormat::Tsv;
            continue;
        }
        if (arg == L"--table") {
            options.format = OutputFormat::Table;
            continue;
        }
        if (arg == L"--output" && i + 1 < argc) {
            std::wstring fmt = argv[++i];
            if (fmt == L"json") options.format = OutputFormat::Json;
            else if (fmt == L"csv") options.format = OutputFormat::Csv;
            else if (fmt == L"tsv") options.format = OutputFormat::Tsv;
            else if (fmt == L"table") options.format = OutputFormat::Table;
            continue;
        }
        if (arg == L"--pipe" && i + 1 < argc) {
            options.pipe_command = argv[++i];
            continue;
        }
        if ((arg == L"-s" || arg == L"--seconds") && i + 1 < argc) {
            wchar_t* endp = nullptr;
            double val = std::wcstod(argv[++i], &endp);
            options.explicit_seconds += val;
            options.has_explicit = true;
            continue;
        }
        if ((arg == L"-m" || arg == L"--minutes") && i + 1 < argc) {
            wchar_t* endp = nullptr;
            double val = std::wcstod(argv[++i], &endp);
            options.explicit_seconds += (val * 60.0);
            options.has_explicit = true;
            continue;
        }
        if ((arg == L"--hours") && i + 1 < argc) {
            wchar_t* endp = nullptr;
            double val = std::wcstod(argv[++i], &endp);
            options.explicit_seconds += (val * 3600.0);
            options.has_explicit = true;
            continue;
        }
        if ((arg == L"-d" || arg == L"--days") && i + 1 < argc) {
            wchar_t* endp = nullptr;
            double val = std::wcstod(argv[++i], &endp);
            options.explicit_seconds += (val * 86400.0);
            options.has_explicit = true;
            continue;
        }
        if ((arg == L"--ms" || arg == L"--millis") && i + 1 < argc) {
            wchar_t* endp = nullptr;
            double val = std::wcstod(argv[++i], &endp);
            options.explicit_seconds += (val * 0.001);
            options.has_explicit = true;
            continue;
        }
        if (arg == L"--") {
            for (++i; i < argc; ++i) {
                options.raw_delays.push_back(argv[i]);
            }
            break;
        }

        options.raw_delays.push_back(arg);
    }

    return true;
}
