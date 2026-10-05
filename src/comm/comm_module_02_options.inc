    struct Options {
        std::string file1;
        std::string file2;
        bool show_col1 = true;         // Unique to file1
        bool show_col2 = true;         // Unique to file2
        bool show_col3 = true;         // Common to both
        bool check_order = true;       // Check input sort order
        bool ignore_case = false;      // Case-insensitive comparisons
        bool show_total = false;       // Summary count of lines
        char delimiter = '\n';         // Line separator (default '\n', or '\0' for -z)
        std::string col_delimiter = "\t"; // Column separator (default TAB)
        bool show_help = false;
        bool show_version = false;
    };

    // =========================================================================
    // String Comparator Utility
    // =========================================================================
