#include "options.hpp"
#include <iostream>
#include <cstdlib>
#include <cwchar>

namespace {
bool parseFormat(const std::wstring& value, MachinfoOptions::OutputFormat& format) {
    if (_wcsicmp(value.c_str(), L"table") == 0) format = MachinfoOptions::OutputFormat::Table;
    else if (_wcsicmp(value.c_str(), L"csv") == 0) format = MachinfoOptions::OutputFormat::Csv;
    else if (_wcsicmp(value.c_str(), L"json") == 0) format = MachinfoOptions::OutputFormat::Json;
    else return false;
    return true;
}

bool applySection(const std::wstring& filter, MachinfoOptions& opts) {
    if (_wcsicmp(filter.c_str(), L"all") == 0) opts.showAll = true;
    else if (_wcsicmp(filter.c_str(), L"bios") == 0 ||
             _wcsicmp(filter.c_str(), L"firmware") == 0 ||
             _wcsicmp(filter.c_str(), L"machine") == 0) opts.showBios = true;
    else if (_wcsicmp(filter.c_str(), L"cpu") == 0 ||
             _wcsicmp(filter.c_str(), L"processor") == 0) opts.showCpu = true;
    else if (_wcsicmp(filter.c_str(), L"memory") == 0 ||
             _wcsicmp(filter.c_str(), L"ram") == 0) opts.showMemory = true;
    else if (_wcsicmp(filter.c_str(), L"tpm") == 0 ||
             _wcsicmp(filter.c_str(), L"security") == 0) opts.showTpm = true;
    else if (_wcsicmp(filter.c_str(), L"os") == 0 ||
             _wcsicmp(filter.c_str(), L"system") == 0) opts.showOs = true;
    else return false;
    return true;
}
}

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

        Section filters
            bios, firmware, machine, cpu, processor, memory, ram, tpm,
            security, os, or system may be supplied as positional filters.

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
            The --format=<format> and --output=<format> forms are also accepted.

        --tpm-wmi
            Enable optional TPM WMI metadata enrichment (disabled by default).

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
    bool positionalOnly = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (positionalOnly) {
            opts.filters.push_back(arg);
            continue;
        }
        if (arg == L"--") {
            positionalOnly = true;
            continue;
        }
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
            if (i + 1 >= argc) {
                std::wcerr << L"Missing format value after " << arg << L". Use table, csv, or json.\n";
                return false;
            }
            if (!parseFormat(argv[++i], opts.format)) {
                std::wcerr << L"Invalid output format: " << argv[i] << L". Use table, csv, or json.\n";
                return false;
            }
        } else if (arg.rfind(L"--format=", 0) == 0 || arg.rfind(L"--output=", 0) == 0) {
            const size_t equals = arg.find(L'=');
            if (!parseFormat(arg.substr(equals + 1), opts.format)) {
                std::wcerr << L"Invalid output format: " << arg.substr(equals + 1) << L". Use table, csv, or json.\n";
                return false;
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
    if (!opts.filters.empty()) {
        for (const std::wstring& filter : opts.filters) {
            if (!applySection(filter, opts)) {
                std::wcerr << L"Unknown machinfo section: " << filter
                           << L". Use --help to list valid sections.\n";
                return false;
            }
        }
    }
    return true;
}
