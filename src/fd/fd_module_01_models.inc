namespace Color {
    const std::string RESET     = "\033[0m";
    const std::string BOLD      = "\033[1m";
    const std::string BLUE      = "\033[1;34m";
    const std::string GREEN     = "\033[1;32m";
    const std::string CYAN      = "\033[1;36m";
    const std::string MAGENTA   = "\033[1;35m";
    const std::string RED       = "\033[1;31m";
    const std::string YELLOW    = "\033[1;33m";
    const std::string GRAY      = "\033[90m";
}

enum class CaseMode {
    Smart,
    Sensitive,
    Insensitive
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

struct CompiledPattern {
    std::string rawPattern;
    bool isFixed = false;
    bool caseSensitive = false;
    std::optional<std::regex> regexObj;
};

const std::unordered_set<std::string> DEFAULT_IGNORED_DIRS = {
    ".git", "node_modules", ".vs", "bin", "obj", "target", "vendor", "build", "dist", ".idea", ".vscode"
};

