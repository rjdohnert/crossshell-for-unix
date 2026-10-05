#include "options.hpp"
#include <iostream>

bool LsattrOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--") {
            for (++i; i < argc; ++i) {
                paths.push_back(argv[i] ? argv[i] : L"");
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
        if (arg == L"-R" || arg == L"--recursive") {
            recursive = true;
            continue;
        }
        if (arg == L"--table") {
            format = OutputFormat::Table;
            continue;
        }
        if (arg == L"--csv") {
            format = OutputFormat::Csv;
            continue;
        }
        if (arg == L"--json") {
            format = OutputFormat::Json;
            continue;
        }
        if (arg == L"-") {
            std::wstring token;
            while (std::wcin >> token) {
                paths.push_back(token);
            }
            continue;
        }
        if (!arg.empty() && arg[0] == L'-') {
            std::wcerr << L"lsattr: unknown option -- " << arg << L"\n";
            return false;
        }

        paths.push_back(arg);
    }

    if (paths.empty() && !showHelp && !showVersion) {
        return false;
    }

    return true;
}

void LsattrOptions::PrintUsage(const wchar_t* progName) const {
    (void)progName;
    std::wcout << LR"(lsattr(1)               CrossShell for UNIX Reference Manual                lsattr(1)

NAME
    lsattr - display Windows file attributes as UNIX-style flags

SYNOPSIS
    lsattr [OPTIONS] FILE...

DESCRIPTION
    Reports Windows file attributes using four UNIX-style flags: a for archive,
    h for hidden, s for system, and i for read-only (immutable). A dash means
    that an attribute is not present.

OPTIONS
    -R, --recursive
        Enumerate the immediate contents of directories.

    --table
        Use aligned table output (default).

    --csv
        Emit CSV output.

    --json
        Emit a JSON array.

    -
        Read paths from standard input.

    -h, --help
        Display this comprehensive reference manual and exit.

    -V, --version
        Display version information and exit.

    --
        End options; remaining values are paths.

OUTPUT
    Table output contains Flags and Path columns. CSV fields are flags and path;
    JSON objects contain flags and path properties.

EXAMPLES
    lsattr app.exe
        Display attributes for one file.

    lsattr -R --json build
        Enumerate a directory and emit JSON.

    Get-ChildItem -Name | lsattr - --csv
        Read paths from a PowerShell pipeline and emit CSV.

EXIT STATUS
    0          Help, version, or successful processing.
    1          Invalid paths or failed option parsing; valid rows may still print.

CrossShell for UNIX                                                      lsattr(1)
)";
}

void LsattrOptions::PrintVersion() const {
    std::wcout << L"lsattr 1.0.0\n";
}
