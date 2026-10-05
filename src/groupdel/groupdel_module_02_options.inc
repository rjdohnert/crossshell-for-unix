    // Command-line options model
    struct CommandOptions {
        std::vector<std::string> groupNames;
        bool force = false;          // -f, --force: Suppress error if group doesn't exist
        bool quiet = false;          // -q, --quiet: Suppress non-error output
        bool verbose = false;        // -v, --verbose: Detailed execution logging
        bool showHelp = false;       // -h, --help, /?
        bool showVersion = false;    // -V, --version
        bool readFromStdin = false;  // Read group names from pipeline / STDIN
    };

