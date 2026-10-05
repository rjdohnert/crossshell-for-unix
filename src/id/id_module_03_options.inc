// ============================================================================
// 3. OPTIONS & COMMAND LINE PARSER
// ============================================================================

enum class OutputFormat {
    Standard = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

class IdOptions {
public:
    bool optUser = false;
    bool optGroup = false;
    bool optGroups = false;
    bool optName = false;
    std::wstring targetUser;
    OutputFormat format = OutputFormat::Standard;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--json") { format = OutputFormat::Json; continue; }
            if (arg == L"--csv") { format = OutputFormat::Csv; continue; }
            if (arg == L"--table") { format = OutputFormat::Table; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
            if (arg == L"-u" || arg == L"--user") optUser = true;
            else if (arg == L"-g" || arg == L"--group") optGroup = true;
            else if (arg == L"-G" || arg == L"--groups") optGroups = true;
            else if (arg == L"-n" || arg == L"--name") optName = true;
            else if (arg == L"-r" || arg == L"--real") { /* POSIX compatibility flag */ }
            else if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
                showHelp = true;
                return true;
            } else if (arg == L"--version" || arg == L"-V") {
                showVersion = true;
                return true;
            } else if (arg == L"--") {
                if (i + 1 < argc && targetUser.empty()) targetUser = argv[++i];
                break;
            } else if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"id: invalid option -- " << arg << L"\n";
                return false;
            } else if (targetUser.empty()) {
                targetUser = arg;
            } else {
                std::wcerr << L"id: extra operand '" << arg << L"'\n";
                return false;
            }
        }

        if (optName && !optUser && !optGroup && !optGroups) {
            std::wcerr << L"id: cannot print only names in default format\n";
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* exe) const {
        std::wcout << LR"(id(1)                   CrossShell for UNIX Reference Manual                    id(1)

    NAME
        id - print real and effective user and group IDs and SIDs

    SYNOPSIS
        id [OPTIONS] [USER]

    DESCRIPTION
        id prints user and group information for the current user, or for the
        specified USER on the local Windows machine or domain.

    OPTIONS
        -u, --user
            Print only the effective user ID / SID.

        -g, --group
            Print only the effective group ID / SID.

        -G, --groups
            Print all group IDs / SIDs.

        -n, --name
            Print a name instead of a number/SID for -u, -g, -G.

        -r, --real
            Print the real ID instead of the effective ID.

        --json, --csv, --table
            Format user and group identity data as JSON, CSV, or table.

        --pipe COMMAND
            Stream output into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        id
            Print full identity information for current session.

        id -Gn Administrator
            Print all group names for Administrator.

    CrossShell for UNIX                                                     id(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"id 1.0.0\n";
    }
};

