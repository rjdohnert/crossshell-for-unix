class ScopedFileHandle {
public:
    explicit ScopedFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedFileHandle() { Close(); }

    ScopedFileHandle(const ScopedFileHandle&) = delete;
    ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;

    ScopedFileHandle(ScopedFileHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = INVALID_HANDLE_VALUE; }
    ScopedFileHandle& operator=(ScopedFileHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

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

class ScopedFindVolumeHandle {
public:
    explicit ScopedFindVolumeHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedFindVolumeHandle() { Close(); }

    ScopedFindVolumeHandle(const ScopedFindVolumeHandle&) = delete;
    ScopedFindVolumeHandle& operator=(const ScopedFindVolumeHandle&) = delete;

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            FindVolumeClose(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedLibraryHandle {
public:
    explicit ScopedLibraryHandle(HMODULE handle = nullptr) : m_handle(handle) {}
    ~ScopedLibraryHandle() { Close(); }

    ScopedLibraryHandle(const ScopedLibraryHandle&) = delete;
    ScopedLibraryHandle& operator=(const ScopedLibraryHandle&) = delete;

    HMODULE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            FreeLibrary(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HMODULE m_handle;
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

typedef VOID(WINAPI* PFMIFSCALLBACK)(ULONG Command, ULONG Action, PVOID Data);
typedef BOOLEAN(WINAPI* PFORMATEX)(
    PWCHAR DriveRoot,
    ULONG MediaFormat,
    PWCHAR Format,
    PWCHAR Label,
    BOOLEAN Quick,
    ULONG ClusterSize,
    PFMIFSCALLBACK Callback
);

// Callback structure for native FormatEx progress output
static VOID WINAPI FormatCallback(ULONG Command, ULONG Action, PVOID Data) {
    if (Command == 0x09) { // FMIFS_PROGRESS
        ULONG percent = *(ULONG*)Data;
        std::wcout << L"\rFormatting... " << percent << L"%" << std::flush;
    }
}

