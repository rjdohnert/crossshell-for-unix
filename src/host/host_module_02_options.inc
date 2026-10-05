// ============================================================================
// 2. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class HostOptions {
public:
    std::wstring target;
    OutputFormat format = OutputFormat::Standard;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::wstring option = argv[i] ? argv[i] : L"";
            if (option == L"--json") {
                format = OutputFormat::Json;
            } else if (option == L"--csv") {
                format = OutputFormat::Csv;
            } else if (option == L"--table") {
                format = OutputFormat::Table;
            } else if (option == L"--pipe" && i + 1 < argc) {
                pipeCommand = argv[++i];
            } else if (option == L"-h" || option == L"--help") {
                showHelp = true;
                return true;
            } else if (option == L"-v" || option == L"-V" || option == L"--version") {
                showVersion = true;
                return true;
            } else if (target.empty() && !option.empty() && option[0] != L'-') {
                target = option;
            } else if (!option.empty() && option[0] == L'-') {
                std::wcerr << L"host: unrecognized option: " << option << L"\n";
                return false;
            }
        }

        if (target.empty() && !showHelp && !showVersion) {
            return false;
        }

        return true;
    }

    void PrintUsage(const wchar_t* programName) const {
        std::wcout << LR"(host(1)                 CrossShell for UNIX Reference Manual                  host(1)

    NAME
        host - DNS lookup utility

    SYNOPSIS
        host [OPTIONS] NAME [SERVER]

    DESCRIPTION
        host is a simple utility for performing DNS lookups. It is normally used
        to convert names to IP addresses and vice versa.

    OPTIONS
        -a, --all
            Query all standard DNS records.

        -t, --type TYPE
            Specify query type (A, AAAA, MX, NS, CNAME, PTR, TXT).

        --json, --csv, --table
            Format DNS query results as JSON, CSV, or table.

        --pipe COMMAND
            Stream output into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        host example.com
            Lookup IP address for example.com.

        host -t MX google.com 8.8.8.8
            Query Google DNS for MX records.

    CrossShell for UNIX                                                   host(1)
)";
    }

    void PrintVersion() const {
        std::wcout << L"host 1.0.0\n";
    }
};

