// ============================================================================
// 1. CONSOLE CONFIGURATION & RAII HANDLES
// ============================================================================

class ConsoleManager {
public:
    static void ConfigureNativePipes() {
        HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode = 0;
        const bool stdoutConsole = GetFileType(stdoutHandle) == FILE_TYPE_CHAR && GetConsoleMode(stdoutHandle, &consoleMode);
        _setmode(_fileno(stdout), stdoutConsole ? _O_U16TEXT : _O_U8TEXT);
        _setmode(_fileno(stderr), _O_U8TEXT);
    }
};

class ScopedDevInfo {
private:
    HDEVINFO m_handle = INVALID_HANDLE_VALUE;

public:
    explicit ScopedDevInfo(HDEVINFO handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedDevInfo() {
        if (isValid()) {
            SetupDiDestroyDeviceInfoList(m_handle);
        }
    }

    bool isValid() const {
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
    }

    HDEVINFO get() const { return m_handle; }

    ScopedDevInfo(const ScopedDevInfo&) = delete;
    ScopedDevInfo& operator=(const ScopedDevInfo&) = delete;

    ScopedDevInfo(ScopedDevInfo&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    ScopedDevInfo& operator=(ScopedDevInfo&& other) noexcept {
        if (this != &other) {
            if (isValid()) SetupDiDestroyDeviceInfoList(m_handle);
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
};

