    // Configuration / Argument options model
    struct CommandOptions {
        std::string groupName;                  // Target group to modify
        std::optional<std::string> newName;     // -n, --new-name NEW_GROUP
        std::optional<std::string> comment;     // -c, --comment COMMENT
        bool force = false;                     // -f, --force
        bool quiet = false;                     // -q, --quiet
        bool verbose = false;                   // -v, --verbose
        bool showHelp = false;                  // -h, --help, /?
        bool showVersion = false;               // -V, --version
        bool readFromStdin = false;             // Read modifications from pipeline / STDIN
    };

