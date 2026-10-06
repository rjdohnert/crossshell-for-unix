#include "options.hpp"
#include <iostream>
#include <cstdlib>

void MachinfoOptions::printHelp(const wchar_t* /*progName*/) {
    std::wcout << LR"(machinfo(1)             CrossShell for UNIX Reference Manual                 machinfo(1)

    NAME
        machinfo - display machine firmware, CPU topology, memory, TPM, and OS details

    SYNOPSIS
        machinfo [OPTION]...

    DESCRIPTION
        Displays comprehensive system and hardware information including firmware version,
        CPU topology and capabilities, physical memory, Trusted Platform Module (TPM) status,
        and operating system details. This implementation accepts the HP-UX-style qualifiers
        commonly used for machine inventory and diagnostics on legacy systems.

    OPTIONS
        -a, --all
            Show the complete machine summary (default behavior).

        -b, --bios
            Display BIOS and system firmware details.

        -c, --cpu
            Display CPU topology and processor-related data.

        -m, --memory
            Display memory and paging summary information.

        -t, --tpm
            Display TPM status and version details.

        -v, --verbose, --extended, -x
            Show extended processor capability details including cache levels and
            virtualization features.

        -q, --quiet, --brief, -s, --summary
            Show condensed summary output (host, CPU, memory, firmware, TPM, OS).

        -f, --format, --output, -o <format>
            Select output format explicitly: table, csv, or json.

        --tpm-wmi
            Enable TPM WMI enrichment (disabled by default for stability).

        --table
            Display aligned summary output (default).

        --csv
            Display CSV-formatted output.

        --json
            Display JSON-formatted output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        machinfo
            Display full system information.

        machinfo -q
            Display condensed system summary.

        machinfo -v
            Show extended processor details.

        machinfo --bios
            Display BIOS and firmware details.

        machinfo --cpu --format csv
            Display processor information in CSV form.

        machinfo --json
            Output system information in JSON format.

    CrossShell for UNIX                                                   machinfo(1)
)";
}

bool MachinfoOptions::parse(int argc, wchar_t* argv[], MachinfoOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-h" || arg == L"--h" || arg == L"--help" || arg == L"/?" || arg == L"-?" || arg == L"--?" || arg == L"/help") {
            printHelp(argv[0]);
            std::exit(0);
        } else if (arg == L"-V" || arg == L"--version") {
            std::wcout << L"machinfo 1.0.0\n";
            std::exit(0);
        } else if (arg == L"-v" || arg == L"--verbose" || arg == L"--extended" || arg == L"-x") {
            opts.verbose = true;
        } else if (arg == L"-q" || arg == L"--quiet" || arg == L"--brief" || arg == L"-s" || arg == L"--summary") {
            opts.quiet = true;
        } else if (arg == L"-a" || arg == L"--all") {
            opts.showAll = true;
            opts.quiet = false;
        } else if (arg == L"-b" || arg == L"--bios") {
            opts.showBios = true;
        } else if (arg == L"-c" || arg == L"--cpu") {
            opts.showCpu = true;
        } else if (arg == L"-m" || arg == L"--memory") {
            opts.showMemory = true;
        } else if (arg == L"-t" || arg == L"--tpm") {
            opts.showTpm = true;
        } else if (arg == L"-f" || arg == L"--format" || arg == L"-o" || arg == L"--output") {
            if (i + 1 < argc) {
                std::wstring next = argv[++i];
                if (next == L"csv") opts.format = OutputFormat::Csv;
                else if (next == L"json") opts.format = OutputFormat::Json;
                else opts.format = OutputFormat::Table;
            }
        } else if (arg == L"--tpm-wmi") {
            opts.tpmWmi = true;
        } else if (arg == L"--table") {
            opts.format = OutputFormat::Table;
        } else if (arg == L"--csv") {
            opts.format = OutputFormat::Csv;
        } else if (arg == L"--json") {
            opts.format = OutputFormat::Json;
        } else if (arg == L"-") {
            std::wstring filter;
            while (std::wcin >> filter) opts.filters.push_back(filter);
        } else if (!arg.empty() && arg[0] != L'-') {
            opts.filters.push_back(arg);
        } else {
            std::wcerr << L"Unknown option: " << arg << L"\nUse -h for help.\n";
            return false;
        }
    }
    return true;
}
