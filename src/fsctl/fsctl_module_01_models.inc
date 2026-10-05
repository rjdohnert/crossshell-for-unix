// ============================================================================
// 1. RAII RESOURCE GUARDS & DATA MODELS
// ============================================================================

class ScopedInternetHandle {
public:
    explicit ScopedInternetHandle(HINTERNET handle = nullptr) : m_handle(handle) {}
    ~ScopedInternetHandle() { Close(); }

    ScopedInternetHandle(const ScopedInternetHandle&) = delete;
    ScopedInternetHandle& operator=(const ScopedInternetHandle&) = delete;

    ScopedInternetHandle(ScopedInternetHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }
    ScopedInternetHandle& operator=(ScopedInternetHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HINTERNET Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            InternetCloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HINTERNET m_handle;
};

class ScopedFindFileHandle {
public:
    explicit ScopedFindFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedFindFileHandle() { Close(); }

    ScopedFindFileHandle(const ScopedFindFileHandle&) = delete;
    ScopedFindFileHandle& operator=(const ScopedFindFileHandle&) = delete;

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            FindClose(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedFileHandle {
public:
    explicit ScopedFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedFileHandle() { Close(); }

    ScopedFileHandle(const ScopedFileHandle&) = delete;
    ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedEnumHandle {
public:
    explicit ScopedEnumHandle(HANDLE handle = nullptr) : m_handle(handle) {}
    ~ScopedEnumHandle() { Close(); }

    ScopedEnumHandle(const ScopedEnumHandle&) = delete;
    ScopedEnumHandle& operator=(const ScopedEnumHandle&) = delete;

    HANDLE Get() const { return m_handle; }
    HANDLE* AddressOf() { return &m_handle; }
    bool IsValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (IsValid()) {
            WNetCloseEnum(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedSid {
public:
    explicit ScopedSid(PSID sid = nullptr) : m_sid(sid) {}
    ~ScopedSid() { Close(); }

    ScopedSid(const ScopedSid&) = delete;
    ScopedSid& operator=(const ScopedSid&) = delete;

    PSID Get() const { return m_sid; }
    bool IsValid() const { return m_sid != nullptr; }

    void Close() {
        if (IsValid()) {
            FreeSid(m_sid);
            m_sid = nullptr;
        }
    }

private:
    PSID m_sid;
};

class ScopedConsoleMode {
public:
    static void EnableVirtualTerminalProcessing() {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
    }
};

class ScopedOutputRedirect {
public:
    ScopedOutputRedirect() = default;
    ~ScopedOutputRedirect() { Cleanup(); }

    bool Redirect(const std::wstring& filePath, bool append) {
        Cleanup();
        if (!append) {
            std::ofstream trunc(filePath, std::ios::trunc);
            trunc.close();
        }
        m_redirect.open(filePath, std::ios::app);

        if (m_redirect.is_open()) {
            m_oldCout = std::cout.rdbuf(m_redirect.rdbuf());
        }
        return m_redirect.is_open();
    }

    void Cleanup() {
        if (m_oldCout) {
            std::cout.rdbuf(m_oldCout);
            m_oldCout = nullptr;
        }
        if (m_redirect.is_open()) {
            m_redirect.close();
        }
    }

private:
    std::ofstream m_redirect;
    std::streambuf* m_oldCout = nullptr;
};

struct RemoteLocation {
    std::wstring scheme;
    std::wstring host;
    std::wstring user;
    std::wstring password;
    std::wstring path;
};

struct TreeItem {
    std::wstring name;
    bool isDir;
};

