// ============================================================================
// Configuration & Options (Unix-Style Qualifiers)
// ============================================================================

struct RobocopyOptions {
    std::string source;
    std::string destination;
    std::vector<std::string> file_specs = {"*"};

    // Directory Tree Options
    bool subdirs = false;             // -s, --subdirs (non-empty)
    bool all_subdirs = false;         // -e, --all-subdirs (includes empty)
    bool mirror = false;              // -m, --mirror (all_subdirs + purge)
    bool purge = false;               // --purge (delete dest files/dirs not in src)
    int max_level = -1;               // -l, --level=N

    // File Selection Options
    bool exclude_changed = false;     // --exclude-changed
    bool exclude_newer = false;       // --exclude-newer
    bool exclude_older = false;       // --exclude-older
    bool exclude_extra = false;       // --exclude-extra
    uintmax_t max_size = UINTMAX_MAX; // --max-size=BYTES
    uintmax_t min_size = 0;           // --min-size=BYTES
    std::vector<std::string> exclude_files; // --exclude-files=PATTERN
    std::vector<std::string> exclude_dirs;  // --exclude-dirs=PATTERN

    // Move & Action Options
    bool move_files = false;          // --move-files
    bool move_all = false;            // --move-all
    bool create_zero = false;         // --create-zero (0-byte files only)

    // Retry & Resiliency
    int retries = 3;                  // -r, --retries=N (default: 3)
    int wait_seconds = 5;             // -w, --wait=N (default: 5s)
    int threads = 1;                  // -j, -t, --threads=N (default: 1)

    // Logging & Diagnostics
    bool dry_run = false;             // -n, -L, --dry-run (list only)
    bool verbose = false;             // -v, --verbose
    bool no_progress = false;         // --no-progress
    bool no_file_list = false;        // --no-file-list
    bool no_dir_list = false;         // --no-dir-list
    bool no_header = false;           // --no-header
    bool no_summary = false;          // --no-summary
    bool show_help = false;
    bool show_version = false;
};

