    struct Options {
        std::string file1;
        std::string file2;
        uint64_t skip1 = 0;
        uint64_t skip2 = 0;
        uint64_t limit_bytes = UINT64_MAX;
        bool verbose_all = false;    // -l: Output byte numbers and differing byte values
        bool print_bytes = false;    // -b: Print differing bytes (octal / chars)
        bool silent = false;         // -s, -q: Suppress output, return status only
        bool show_help = false;
        bool show_version = false;
    };

    // =========================================================================
    // IByteStream & Stream Implementations (Abstraction Layer for File / Stdin)
    // =========================================================================
