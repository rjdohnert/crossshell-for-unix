#include "engine.hpp"

// ============================================================================
// WinsockScope & ScopedSocket
// ============================================================================

WinsockScope::WinsockScope() : m_initialized(false) {
    WSADATA wsaData = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        m_initialized = true;
    }
}

WinsockScope::~WinsockScope() {
    if (m_initialized) {
        WSACleanup();
    }
}

ScopedSocket::ScopedSocket(SOCKET sock) : m_socket(sock) {}

ScopedSocket::~ScopedSocket() {
    Close();
}

ScopedSocket::ScopedSocket(ScopedSocket&& other) noexcept : m_socket(other.m_socket) {
    other.m_socket = INVALID_SOCKET;
}

ScopedSocket& ScopedSocket::operator=(ScopedSocket&& other) noexcept {
    if (this != &other) {
        Close();
        m_socket = other.m_socket;
        other.m_socket = INVALID_SOCKET;
    }
    return *this;
}

void ScopedSocket::Close() {
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}

void ScopedSocket::Reset(SOCKET sock) {
    Close();
    m_socket = sock;
}

// ============================================================================
// FingerClient
// ============================================================================

bool FingerClient::QueryRemote(const std::string& user, const std::string& host) {
    WinsockScope winsock;
    if (!winsock.IsInitialized()) {
        std::cerr << "pinky: Winsock initialization failed\n";
        return false;
    }

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), "79", &hints, &result) != 0) {
        std::cerr << "pinky: unknown host: " << host << "\n";
        return false;
    }

    ScopedSocket sock;
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        SOCKET s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (s == INVALID_SOCKET) continue;

        if (connect(s, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == SOCKET_ERROR) {
            closesocket(s);
            continue;
        }

        sock.Reset(s);
        break;
    }

    freeaddrinfo(result);

    if (!sock.IsValid()) {
        std::cerr << "pinky: connect failed to " << host << " on port 79\n";
        return false;
    }

    std::string request = user + "\r\n";
    send(sock.Get(), request.c_str(), static_cast<int>(request.length()), 0);

    char buffer[512];
    int bytesReceived = 0;
    while ((bytesReceived = recv(sock.Get(), buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::cout << buffer;
    }

    return true;
}

// ============================================================================
// PinkyProfileService
// ============================================================================

std::wstring PinkyProfileService::FormatTime(DWORD timeSecs) {
    if (timeSecs == 0) return L"-";
    time_t t = static_cast<time_t>(timeSecs);
    tm tmLocal;
    localtime_s(&tmLocal, &t);
    wchar_t buf[64];
    wcsftime(buf, sizeof(buf) / sizeof(wchar_t), L"%b %d %H:%M", &tmLocal);
    return buf;
}

std::string PinkyProfileService::WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) return {};
    std::string utf8(static_cast<size_t>(required - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &utf8[0], required, nullptr, nullptr);
    return utf8;
}

std::wstring PinkyProfileService::GetUserFullName(const std::wstring& username) {
    LPUSER_INFO_10 pBuf = nullptr;
    if (NetUserGetInfo(nullptr, username.c_str(), 10, reinterpret_cast<LPBYTE*>(&pBuf)) == NERR_Success && pBuf) {
        ScopedNetApiMemory<USER_INFO_10> bufHolder(pBuf);
        if (pBuf->usri10_full_name && wcslen(pBuf->usri10_full_name) > 0) {
            return pBuf->usri10_full_name;
        }
    }
    return username;
}

std::vector<PinkySessionSummary> PinkyProfileService::QueryActiveSessions() {
    std::vector<PinkySessionSummary> list;
    PWTS_SESSION_INFOW pSessionInfo = nullptr;
    DWORD count = 0;

    if (!WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &count)) {
        return list;
    }
    ScopedWtsMemory<WTS_SESSION_INFOW> sessionHolder(pSessionInfo);

    for (DWORD i = 0; i < count; ++i) {
        PWSTR pUserName = nullptr;
        PWSTR pWinStation = nullptr;
        PWSTR pClientName = nullptr;
        DWORD bytes = 0;

        WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionHolder.Get()[i].SessionId, WTSUserName, &pUserName, &bytes);
        ScopedWtsMemory<wchar_t> userHolder(pUserName);
        if (!pUserName || wcslen(pUserName) == 0) {
            continue;
        }

        WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionHolder.Get()[i].SessionId, WTSWinStationName, &pWinStation, &bytes);
        ScopedWtsMemory<wchar_t> stationHolder(pWinStation);

        WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionHolder.Get()[i].SessionId, WTSClientName, &pClientName, &bytes);
        ScopedWtsMemory<wchar_t> clientHolder(pClientName);

        PWTSINFOW pInfo = nullptr;
        std::wstring logonTime = L"-";
        if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, sessionHolder.Get()[i].SessionId, WTSSessionInfo, reinterpret_cast<PWSTR*>(&pInfo), &bytes)) {
            ScopedWtsMemory<WTSINFOW> infoHolder(pInfo);
            if (pInfo && pInfo->LogonTime.QuadPart > 0) {
                FILETIME ft;
                ft.dwLowDateTime = pInfo->LogonTime.LowPart;
                ft.dwHighDateTime = pInfo->LogonTime.HighPart;
                SYSTEMTIME stUTC, stLocal;
                FileTimeToSystemTime(&ft, &stUTC);
                SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);
                wchar_t tbuf[64];
                swprintf_s(tbuf, L"%02d:%02d", stLocal.wHour, stLocal.wMinute);
                logonTime = tbuf;
            }
        }

        PinkySessionSummary summary;
        summary.username = pUserName;
        summary.fullName = GetUserFullName(pUserName);
        summary.line = (pWinStation && wcslen(pWinStation) > 0) ? pWinStation : L"console";
        summary.idle = (sessionHolder.Get()[i].State == WTSActive) ? L"*" : L"idle";
        summary.logonTime = logonTime;
        summary.host = (pClientName && wcslen(pClientName) > 0) ? pClientName : L"local";

        list.push_back(summary);
    }

    return list;
}

PinkyUserProfile PinkyProfileService::QueryUserProfile(const std::wstring& username, bool printPlan) {
    PinkyUserProfile profile;
    profile.username = username;

    LPUSER_INFO_2 pBuf = nullptr;
    NET_API_STATUS status = NetUserGetInfo(nullptr, username.c_str(), 2, reinterpret_cast<LPBYTE*>(&pBuf));

    if (status != NERR_Success || !pBuf) {
        profile.userFound = false;
        return profile;
    }
    ScopedNetApiMemory<USER_INFO_2> bufHolder(pBuf);

    profile.userFound = true;
    profile.fullName = (pBuf->usri2_full_name && wcslen(pBuf->usri2_full_name) > 0) ? pBuf->usri2_full_name : username;
    profile.homeDir = (pBuf->usri2_home_dir && wcslen(pBuf->usri2_home_dir) > 0) ? pBuf->usri2_home_dir : (L"C:\\Users\\" + username);
    profile.comment = (pBuf->usri2_comment && wcslen(pBuf->usri2_comment) > 0) ? pBuf->usri2_comment : L"-";
    profile.shell = (pBuf->usri2_script_path && wcslen(pBuf->usri2_script_path) > 0) ? pBuf->usri2_script_path : L"C:\\Windows\\System32\\cmd.exe";
    profile.lastLogon = pBuf->usri2_last_logon;

    if (printPlan) {
        fs::path profilePath = fs::path(L"C:\\Users") / username / L".plan";
        if (!fs::exists(profilePath)) {
            profilePath = fs::path(L"C:\\Users") / username / L"plan.txt";
        }

        if (fs::exists(profilePath)) {
            std::wifstream planFile(profilePath);
            std::wstring line;
            while (std::getline(planFile, line)) {
                profile.planLines.push_back(line);
            }
        }
    }

    return profile;
}
