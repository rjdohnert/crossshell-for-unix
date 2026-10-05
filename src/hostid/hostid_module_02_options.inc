// ============================================================================
// 2. OPTIONS & COMMAND LINE PARSER
// ============================================================================

enum class OutputFormat {
    Standard = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

class HostidOptions {
public:
    OutputFormat format = OutputFormat::Standard;
    std::string pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "--json") {
                format = OutputFormat::Json;
            } else if (arg == "--csv") {
                format = OutputFormat::Csv;
            } else if (arg == "--table") {
                format = OutputFormat::Table;
            } else if (arg == "--pipe" && i + 1 < argc) {
                pipeCommand = argv[++i];
            } else if (arg == "--help" || arg == "-h" || arg == "/?") {
                showHelp = true;
                return true;
            } else if (arg == "--version" || arg == "-V") {
                showVersion = true;
                return true;
            } else if (arg == "--") {
                for (++i; i < argc; ++i) {
                    std::cerr << "hostid: extra operand '" << argv[i] << "'\n";
                    return false;
                }
                break;
            } else {
                std::cerr << "hostid: invalid option -- '" << arg << "'\n";
                return false;
            }
        }
        return true;
    }

    void PrintUsage() const {
        std::cout << R"(hostid(1)               CrossShell for UNIX Reference Manual                hostid(1)

    NAME
        hostid - print the numeric identifier for the current host

    SYNOPSIS
        hostid [OPTIONS]

    DESCRIPTION
        hostid prints the numeric identifier (in hexadecimal) for the current host,
        derived from the machine GUID, motherboard UUID, or network interface MAC.

    OPTIONS
        --json, --csv, --table
            Output host identifier as JSON, CSV, or table.

        --pipe COMMAND
            Route output into COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        hostid
            Display host identifier in hex format.

    CrossShell for UNIX                                                 hostid(1)
)";
    }

    void PrintVersion() const {
        std::cout << "hostid 1.0.0\n";
    }
};

