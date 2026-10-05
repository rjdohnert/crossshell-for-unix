// ============================================================================
// 4. OPTIONS & COMMAND LINE PARSER
// ============================================================================

class HostmapOptions {
public:
    bool showSys = false;
    bool showCpu = false;
    bool showMem = false;
    bool showNet = false;
    bool jsonOutput = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        bool explicitSelection = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i] ? argv[i] : "";
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "-a" || arg == "--all") {
                showSys = showCpu = showMem = showNet = true;
                explicitSelection = true;
            } else if (arg == "-s" || arg == "--sys") {
                showSys = true;
                explicitSelection = true;
            } else if (arg == "-c" || arg == "--cpu") {
                showCpu = true;
                explicitSelection = true;
            } else if (arg == "-m" || arg == "--mem") {
                showMem = true;
                explicitSelection = true;
            } else if (arg == "-n" || arg == "--net") {
                showNet = true;
                explicitSelection = true;
            } else if (arg == "-j" || arg == "--json") {
                jsonOutput = true;
            } else {
                std::cerr << "hostmap: unknown option '" << arg << "'\n";
                return false;
            }
        }

        if (!explicitSelection) {
            showSys = showCpu = showMem = showNet = true;
        }

        return true;
    }

    void PrintHelp() const {
        std::cout << R"(hostmap(1)              CrossShell for UNIX Reference Manual               hostmap(1)

    NAME
        hostmap - comprehensive system hardware and network topology mapper

    SYNOPSIS
        hostmap [OPTIONS]

    DESCRIPTION
        hostmap queries and renders detailed system configuration, CPU telemetry,
        memory topology, network adapters, and routing maps.

    OPTIONS
        -a, --all
            Display all subsystem topology information.

        -s, --sys
            Display system and motherboard configuration.

        -c, --cpu
            Display CPU architecture and core topology.

        -m, --mem
            Display physical and virtual memory metrics.

        -n, --net
            Display network adapters and interface maps.

        --json, --csv, --table
            Output structured topology records.

        --pipe COMMAND
            Stream results to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        hostmap -a --json
            Map all hardware subsystems and output as JSON.

    CrossShell for UNIX                                                hostmap(1)
)";
    }

    void PrintVersion() const {
        std::cout << "hostmap 4.9.0\n";
    }
};

