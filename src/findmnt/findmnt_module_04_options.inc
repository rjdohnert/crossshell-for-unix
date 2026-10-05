// ============================================================================
// 4. OPTIONS & REPORTER
// ============================================================================

class FindmntOptions {
public:
    bool noHeadings = false;
    OutputFormat outputFormat = OutputFormat::Default;
    std::wstring pipeCommand;
    std::vector<std::wstring> filters;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
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
            if (arg == L"--json") { outputFormat = OutputFormat::Json; continue; }
            if (arg == L"--csv") { outputFormat = OutputFormat::Csv; continue; }
            if (arg == L"--table") { outputFormat = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
            if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"findmnt: unknown option -- " << arg << L"\n";
                return false;
            }
            filters.push_back(arg);
        }
        return true;
    }

    void PrintHelp(const wchar_t* progName) const {
        std::wcout << LR"(findmnt(1)              CrossShell for UNIX Reference Manual               findmnt(1)

    NAME
        findmnt - find and inspect mounted filesystems and drive volumes

    SYNOPSIS
        findmnt [OPTIONS] [DEVICE|MOUNTPOINT]

    DESCRIPTION
        findmnt lists all mounted filesystems or searches for a filesystem in
        Windows drive tables, network mounts, and volume mount points.

    OPTIONS
        -n, --noheadings
            Do not print a header line.

        -l, --list
            Use list output format.

        -t, --types TYPES
            Limit the set of printed filesystems by filesystem types.

        --json, --csv, --table
            Output filesystem mount information as JSON, CSV, or table.

        --pipe COMMAND
            Send output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        findmnt
            List all active mounts and drive volumes.

        findmnt -t NTFS --json
            List all NTFS mounted volumes in JSON format.

    CrossShell for UNIX                                                findmnt(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"findmnt 1.0.0\n";
    }
};
