    // Configuration / Argument options data model
    struct CommandOptions {
        std::string groupName;
        std::string comment;
        bool force = false;          // -f, --force: Exit successfully if the group exists
        bool systemGroup = false;     // -r, --system: Mark/designate as a system group
        bool quiet = false;           // -q, --quiet: Suppress non-error output
        bool verbose = false;         // -v, --verbose: Detailed execution logging
        bool showHelp = false;        // -h, --help
        bool showVersion = false;     // -V, --version
        bool readFromStdin = false;   // Read group names from pipeline / STDIN
    };

