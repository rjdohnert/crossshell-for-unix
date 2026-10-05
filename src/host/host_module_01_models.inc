// ============================================================================
// 1. RAII SCOPES & DATA STRUCTURES
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

class WinsockScope {
public:
    WinsockScope() : m_initialized(false) {
        WSADATA wsa = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
            m_initialized = true;
        }
    }

    ~WinsockScope() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

class ScopedDnsRecordList {
public:
    explicit ScopedDnsRecordList(PDNS_RECORD record = nullptr) : m_record(record) {}

    ~ScopedDnsRecordList() {
        Free();
    }

    ScopedDnsRecordList(const ScopedDnsRecordList&) = delete;
    ScopedDnsRecordList& operator=(const ScopedDnsRecordList&) = delete;

    ScopedDnsRecordList(ScopedDnsRecordList&& other) noexcept : m_record(other.m_record) {
        other.m_record = nullptr;
    }

    ScopedDnsRecordList& operator=(ScopedDnsRecordList&& other) noexcept {
        if (this != &other) {
            Free();
            m_record = other.m_record;
            other.m_record = nullptr;
        }
        return *this;
    }

    PDNS_RECORD* ReceiveHandle() {
        Free();
        return &m_record;
    }

    PDNS_RECORD Get() const { return m_record; }
    operator PDNS_RECORD() const { return m_record; }

private:
    void Free() {
        if (m_record) {
            DnsRecordListFree(m_record, DnsFreeRecordList);
            m_record = nullptr;
        }
    }

    PDNS_RECORD m_record;
};

enum class OutputFormat {
    Standard = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

struct ResolvedRecord {
    std::wstring query;
    std::wstring type;
    std::wstring value;
    std::wstring formattedLine;
};

