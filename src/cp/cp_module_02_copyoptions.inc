class CopyOptions {
public:
    bool force = false;            // -f: Unlink existing dest without prompting
    bool interactive = false;      // -i: Prompt before overwriting
    bool preserve = false;         // -p: Preserve timestamps, modes, attributes
    bool recursive = false;        // -r, -R: Copy directory subtrees
    bool hpuxExtentAcl = false;    // -e: Preserve Windows attributes / ACLs (HP-UX -e)
    bool noDereference = false;    // -P: Do not follow symlinks
    bool dereference = false;      // -L: Dereference symlinks (default)
    bool showProgress = true;      // Visual progress display
    bool verbose = false;          // -v: Verbose output
    bool outputJson = false;       // --json: Output structured JSON

    std::vector<fs::path> sources;
    fs::path destination;
};

// ============================================================================
// Class: ProgressBar
// Handles visual terminal progress rendering, transfer rate, and ETA.
// ============================================================================
