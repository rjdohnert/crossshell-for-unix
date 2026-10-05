    // Program Exit Codes
    enum class ExitCode : int {
        Success = 0,
        Failure = 1
    };

    enum class SizeFormatMode {
        Kilobytes,     // Default: 1024-byte blocks
        Megabytes,     // -m: 1048576-byte blocks
        Bytes,         // -b: raw bytes
        HumanReadable  // -H: 1K, 234M, 2G
    };

    // Configuration Settings
    struct Options {
        std::vector<std::string> paths;
        bool all_files = false;               // -a, --all
        bool summarize_only = false;          // -s, --summarize
        bool total = false;                   // -c, --total
        bool apparent_size = false;           // -b, --apparent-size
        bool null_terminated = false;         // -0, --null
        bool dereference = false;             // -L, --dereference
        int max_depth = -1;                   // -d N, --max-depth=N (-1 = unlimited)
        SizeFormatMode format_mode = SizeFormatMode::Kilobytes;
        uint64_t block_size = 1024;
        uint64_t cluster_size = 4096;         // Standard Windows cluster size (4 KB)
        std::vector<std::string> exclude_patterns;
        std::string files0_from;
        bool show_help = false;
        bool show_version = false;
        int output_format = 0;
        std::string pipe_command;
    };
