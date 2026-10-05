struct Options {
    std::wstring logName = L"System";
    DWORD maxEntries = 50;
    bool showAll = false;
    bool help = false;
    bool version = false;
    enum class OutputFormat { Table, Json, Csv } output = OutputFormat::Table;
    std::wstring pipeCommand;
};

struct EventEntry {
    std::string timestamp;
    std::string severity;
    DWORD eventId = 0;
    std::string source;
    std::string summary;
};

class ErrptOutputGuard {
    std::streambuf* old_;
    FILE* pipe_;
    std::ostringstream capture_;
public:
    explicit ErrptOutputGuard(const std::wstring& command) : old_(nullptr), pipe_(nullptr) {
        if (!command.empty()) old_ = std::cout.rdbuf(capture_.rdbuf());
        if (!command.empty()) pipe_ = _wpopen(command.c_str(), L"w");
    }
    ~ErrptOutputGuard() {
        if (!old_) return;
        std::cout.rdbuf(old_);
        std::string text = capture_.str();
        if (pipe_) { std::fwrite(text.data(), 1, text.size(), pipe_); _pclose(pipe_); }
        else std::cout << text;
    }
};
