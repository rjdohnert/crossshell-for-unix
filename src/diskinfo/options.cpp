#include "options.hpp"
#include <iostream>

bool DiskinfoOptions::Parse(int argc, wchar_t* argv[]) {
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
            std::wcerr << L"diskinfo: unknown option -- " << arg << L"\n";
            return false;
        }
        filters.push_back(arg);
    }
    return true;
}

void DiskinfoOptions::PrintHelp(const wchar_t* progName) const {
    (void)progName;
    std::wcout << LR"(diskinfo(1)              CrossShell for UNIX Reference Manual                 diskinfo(1)

NAME
    diskinfo - list logical Windows block devices and mount points

SYNOPSIS
    diskinfo [OPTIONS] [FILTER...]

DESCRIPTION
    Enumerates Windows logical drives, including remote drives, and reports
    filesystem, mount-point, size, and read-only information. Filters
    match NAME, TYPE, PKNAME, FSTYPE, MOUNTPOINT, SIZE, or RO.

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
    The default table uses a tree layout and the columns NAME, MAJ:MIN, RM,
    SIZE, RO, TYPE, and MOUNTPOINTS. Windows does not expose Linux major/minor
    device numbers, so MAJ:MIN is shown as "-". CSV fields are NAME, TYPE,
    PKNAME, FSTYPE, MOUNTPOINT, SIZE_BYTES, SIZE, and RO. JSON fields are
    name, type, parent, fsType, mountPoint, sizeBytes, size, and readOnly.

EXAMPLES
    diskinfo
        List all logical drives in table format.

    diskinfo --json C:
        Emit the C: drive record as JSON.

    echo NTFS | diskinfo -
        Read a filesystem filter from standard input.

EXIT STATUS
    0          Help, version, or successful enumeration.
    1          Invalid option or failed argument parsing.

CrossShell for UNIX                                                    diskinfo(1)
)";
}

void DiskinfoOptions::PrintVersion() const {
    std::wcout << L"diskinfo 1.0.0\n";
}
