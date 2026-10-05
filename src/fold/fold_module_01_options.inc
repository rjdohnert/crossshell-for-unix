    // =========================================================================
    // Configuration & Settings
    // =========================================================================
    struct Options {
        size_t width = 80;                     // Default wrap width
        bool count_bytes = false;             // -b, --bytes
        bool break_at_spaces = false;         // -s, --spaces
        std::vector<std::string> files;       // Files to process (empty or "-" means stdin)
        bool show_help = false;
        bool show_version = false;
        int output_format = 0;
        std::string pipe_command;
    };

