// ============================================================================
// 1. DATA MODELS & RAII HANDLERS
// ============================================================================

class NativePipeConfigurator {
public:
    static void Configure() {
        HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode = 0;
        const bool stdoutConsole = (GetFileType(stdoutHandle) == FILE_TYPE_CHAR) && GetConsoleMode(stdoutHandle, &consoleMode);
        _setmode(_fileno(stdout), stdoutConsole ? _O_U16TEXT : _O_U8TEXT);
        _setmode(_fileno(stderr), _O_U8TEXT);
    }
};

class ScopedTokenHandle {
public:
    explicit ScopedTokenHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedTokenHandle() {
        Close();
    }

    ScopedTokenHandle(const ScopedTokenHandle&) = delete;
    ScopedTokenHandle& operator=(const ScopedTokenHandle&) = delete;

    ScopedTokenHandle(ScopedTokenHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedTokenHandle& operator=(ScopedTokenHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    operator HANDLE() const { return m_handle; }
    HANDLE* ReceiveHandle() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    HANDLE m_handle;
};

struct AccountInfo {
    std::wstring sidString;
    std::wstring accountName;
};

struct UserTokenData {
    AccountInfo user;
    AccountInfo primaryGroup;
    std::vector<AccountInfo> groups;
};

