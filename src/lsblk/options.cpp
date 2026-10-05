#include "options.hpp"
#include <iostream>

bool LsblkOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--") {
            for (++i; i < argc; ++i) {
                filters.push_back(argv[i] ? argv[i] : L"");
            }
            break;
        }
        if (arg == L"-h" || arg == L"--help") {
            showHelp = true;
            return true;
        }
        if (arg == L"-V" || arg == L"--version") {
            showVersion = true;
            return true;
        }
        if (arg == L"-n" || arg == L"--noheadings") {
            noHeadings = true;
            continue;
        }
        if (arg == L"--table") { format = OutputFormat::Table; continue; }
        if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
        if (arg == L"--json") { format = OutputFormat::Json; continue; }
        if (arg == L"-") {
            std::wstring token;
            while (std::wcin >> token) filters.push_back(token);
            continue;
        }
        if (!arg.empty() && arg[0] == L'-') {
            std::wcerr << L"lsblk: unknown option -- " << arg << L"\n";
            return false;
        }
        filters.push_back(arg);
    }
    return true;
}

void LsblkOptions::PrintHelp(const wchar_t* progName) const {
    (void)progName;
    std::wcout << LR"(lsblk(1)                CrossShell for UNIX Reference Manual                   lsblk(1)

NAME
    lsblk - list logical Windows block devices and mount points

SYNOPSIS
    lsblk [OPTIONS] [FILTER...]

DESCRIPTION
    Enumerates Windows logical drives, including remote drives, and reports
    filesystem, mount-point, size, read-only, and health information. Filters
    match NAME, TYPE, FSTYPE, MOUNTPOINT, SIZE, RO, or HEALTH.

OPTIONS
    -n, --noheadings
        Suppress the table header row.

    --table
        Use aligned table output (default).

    --csv
        Emit CSV output.

    --json
        Emit a JSON array.

    -
        Read whitespace-delimited filters from standard input.

    -h, --help
        Display this comprehensive reference manual and exit.

    -V, --version
        Display version information and exit.

    --
        End options; remaining arguments are filters.

OUTPUT
    Table and CSV fields are NAME, TYPE, FSTYPE, MOUNTPOINT, SIZE, RO, and
    HEALTH. JSON fields are name, type, fsType, mountPoint, size, readOnly,
    and health.

EXAMPLES
    lsblk
        List all logical drives in table format.

    lsblk --json C:
        Emit the C: drive record as JSON.

    echo NTFS | lsblk -
        Read a filesystem filter from standard input.

EXIT STATUS
    0          Help, version, or successful enumeration.
    1          Invalid option or failed argument parsing.

CrossShell for UNIX                                                       lsblk(1)
)";
}

void LsblkOptions::PrintVersion() const {
    std::wcout << L"lsblk 1.0.0\n";
}
