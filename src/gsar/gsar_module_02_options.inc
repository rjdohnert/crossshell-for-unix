// ============================================================================
// Application Configuration & Options
// ============================================================================
struct Options {
    std::vector<uint8_t> searchPattern;
    std::vector<uint8_t> replacePattern;
    bool hasSearch = false;
    bool hasReplace = false;
    bool caseInsensitive = false;
    bool countOnly = false;
    bool verbose = false;
    bool showOffsets = false;
    bool createBackup = false;
    bool filterPipeMode = false;
    bool displayHex = false;
    std::string backupExtension = ".bak";
    std::vector<std::string> inputFiles;
    std::string outputFile;
};

