// ANSI Terminal Formatting
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

class ConsoleEnvironment {
public:
    static void EnableVirtualTerminal() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
};

class ScopedProcessToken {
public:
    explicit ScopedProcessToken(HANDLE token = nullptr) : m_token(token) {}
    ~ScopedProcessToken() { Close(); }

    ScopedProcessToken(const ScopedProcessToken&) = delete;
    ScopedProcessToken& operator=(const ScopedProcessToken&) = delete;

    HANDLE Get() const { return m_token; }
    bool IsValid() const { return m_token != nullptr && m_token != INVALID_HANDLE_VALUE; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_token);
            m_token = nullptr;
        }
    }

private:
    HANDLE m_token;
};

struct CleanTarget {
    std::string name;
    std::string flag;
    std::string description;
    std::vector<std::wstring> rawPaths;
    bool selected = false;
    bool destructive = false;
    std::string risk = "low";
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

struct DskctlConfig {
    bool dryRun = true;
    bool verbose = false;
    bool force = false;
    bool trash = false;
    bool execute = false;
    OutputFormat outputFormat = OutputFormat::Default;
    std::string pipeCommand;
};

struct CleanSummary {
    uint64_t totalBytesFreed = 0;
    size_t totalFilesRemoved = 0;
    size_t totalSkipped = 0;
    size_t totalErrors = 0;
};
