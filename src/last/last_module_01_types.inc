// Check if process has Administrator rights required for Windows Security log
bool IsUserAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

// RAII Wrapper for EVT_HANDLE
class ScopedEvtHandle {
    EVT_HANDLE m_h;
public:
    ScopedEvtHandle(EVT_HANDLE h = NULL) : m_h(h) {}
    ~ScopedEvtHandle() { if (m_h) EvtClose(m_h); }
    EVT_HANDLE get() const { return m_h; }
    EVT_HANDLE* operator&() { return &m_h; }
    operator EVT_HANDLE() const { return m_h; }
    ScopedEvtHandle& operator=(EVT_HANDLE h) {
        if (m_h) EvtClose(m_h);
        m_h = h;
        return *this;
    }
};

struct EventRecord {
    DWORD eventId;
    ULONGLONG timestamp;
    std::wstring username;
    std::wstring logonId;
    DWORD logonType;
    std::wstring ipOrHost;
};

struct DisplayRecord {
    std::wstring username;
    std::wstring tty;
    std::wstring host;
    ULONGLONG loginTime;
    ULONGLONG logoutTime;
    bool stillLoggedIn;
    bool stillRunning;
    bool wasShutdownTerminated = false;
};
