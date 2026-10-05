    // =========================================================================
    // CLI Options Model
    // =========================================================================
    struct Options {
        bool omit_header = false;          // -c, --omit-header
        bool show_default = true;          // -d, --default (inherited directory ACLs)
        bool show_access = true;           // -a, --access (base and explicit DACLs)
        bool numeric_ids = false;          // -n, --numeric (print raw SIDs instead of names)
        bool skip_base_only = false;       // -s, --skip-base (skip files with only standard owner/group rights)
        bool recursive = false;            // -R, --recursive
        bool follow_symlinks = false;      // -L, --logical vs -P, --physical
        bool tabular = false;              // -t, --tabular
        bool read_stdin = false;           // - (read file list from stdin)
        bool show_help = false;
        bool show_version = false;
        std::vector<std::string> paths;
    };

