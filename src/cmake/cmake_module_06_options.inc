struct Options {
    Mode mode = Mode::ConfigureGenerate;

    // Configuration / Generation
    fs::path sourceDir = fs::current_path();
    fs::path binaryDir = fs::current_path();
    std::string generator = "Visual Studio 17 2022";
    std::string architecture;
    std::string toolset;
    std::map<std::string, CacheEntry> cacheDefines;
    std::vector<std::string> cacheUndefines;
    std::string initialCacheScript;
    bool fresh = false;

    // Build mode options
    fs::path buildDir;
    std::string target;
    std::string config = "Debug";
    bool cleanFirst = false;
    int parallelJobs = 0;
    bool verbose = false;
    std::vector<std::string> nativeToolArgs;

    // Script mode options
    fs::path scriptPath;
};

// ============================================================================
// Subprocess Execution Engine (Strict Win32 Conformance)
// ============================================================================
