    struct GroupInfo {
        std::string name;
        std::string sid;
        bool isLocal = true;

        bool operator<(const GroupInfo& other) const {
            return name < other.name;
        }
    };

    // Command-line configuration options
    struct CommandOptions {
        std::vector<std::string> usernames;
        std::string delimiter = " ";      // Default delimiter between group names
        bool showSid = false;             // -s, --sid: Show SID alongside group name
        bool showType = false;            // -t, --type: Distinguish [Local] vs [Global]
        bool quiet = false;               // -q, --quiet: Suppress non-error output
        bool verbose = false;             // -v, --verbose: Detailed execution logging
        bool showHelp = false;            // -h, --help, /?
        bool showVersion = false;         // -V, --version
        bool readFromStdin = false;       // Read usernames from pipeline / STDIN
    };

    // Windows User & Group Query Manager
