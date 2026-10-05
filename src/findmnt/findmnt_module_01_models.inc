// ============================================================================
// 1. RAII GUARDS & DATA MODELS
// ============================================================================

class ScopedWpOpen {
public:
    explicit ScopedWpOpen(FILE* pipe = nullptr) : m_pipe(pipe) {}
    ~ScopedWpOpen() { Close(); }

    ScopedWpOpen(const ScopedWpOpen&) = delete;
    ScopedWpOpen& operator=(const ScopedWpOpen&) = delete;

    ScopedWpOpen(ScopedWpOpen&& other) noexcept : m_pipe(other.m_pipe) { other.m_pipe = nullptr; }
    ScopedWpOpen& operator=(ScopedWpOpen&& other) noexcept {
        if (this != &other) {
            Close();
            m_pipe = other.m_pipe;
            other.m_pipe = nullptr;
        }
        return *this;
    }

    FILE* Get() const { return m_pipe; }
    bool IsValid() const { return m_pipe != nullptr; }

    void Close() {
        if (m_pipe) {
            _pclose(m_pipe);
            m_pipe = nullptr;
        }
    }

private:
    FILE* m_pipe;
};

struct MountEntry {
    std::wstring source;
    std::wstring target;
    std::wstring fsType;
    std::wstring options;
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

