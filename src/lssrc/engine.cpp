#include "engine.hpp"
#include <algorithm>
#include <cwctype>

ScopedServiceHandle::ScopedServiceHandle(SC_HANDLE handle) : m_handle(handle) {}

ScopedServiceHandle::~ScopedServiceHandle() {
    Close();
}

ScopedServiceHandle::ScopedServiceHandle(ScopedServiceHandle&& other) noexcept : m_handle(other.m_handle) {
    other.m_handle = nullptr;
}

ScopedServiceHandle& ScopedServiceHandle::operator=(ScopedServiceHandle&& other) noexcept {
    if (this != &other) {
        Close();
        m_handle = other.m_handle;
        other.m_handle = nullptr;
    }
    return *this;
}

SC_HANDLE ScopedServiceHandle::Get() const {
    return m_handle;
}

ScopedServiceHandle::operator SC_HANDLE() const {
    return m_handle;
}

bool ScopedServiceHandle::IsValid() const {
    return m_handle != nullptr;
}

void ScopedServiceHandle::Close() {
    if (m_handle) {
        CloseServiceHandle(m_handle);
        m_handle = nullptr;
    }
}

void ScopedServiceHandle::Reset(SC_HANDLE handle) {
    Close();
    m_handle = handle;
}

std::wstring ServiceManagerEngine::ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return s;
}

std::string ServiceManagerEngine::ToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return "";
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], size, nullptr, nullptr);
    return out;
}

const char* ServiceManagerEngine::StateToWord(DWORD state) {
    switch (state) {
        case SERVICE_RUNNING: return "active";
        case SERVICE_STOPPED: return "inoperative";
        case SERVICE_START_PENDING: return "starting";
        case SERVICE_STOP_PENDING: return "stopping";
        case SERVICE_PAUSED: return "paused";
        default: return "unknown";
    }
}

bool ServiceManagerEngine::EnumerateServices(bool includeStopped, std::vector<ServiceRow>& outRows, std::string& outError) {
    ScopedServiceHandle scm(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
    if (!scm.IsValid()) {
        outError = "cannot open Service Control Manager (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    DWORD needed = 0;
    DWORD returned = 0;
    DWORD resume = 0;
    DWORD serviceType = SERVICE_WIN32;
    DWORD serviceState = includeStopped ? SERVICE_STATE_ALL : SERVICE_ACTIVE;

    EnumServicesStatusExW(
        scm.Get(),
        SC_ENUM_PROCESS_INFO,
        serviceType,
        serviceState,
        nullptr,
        0,
        &needed,
        &returned,
        &resume,
        nullptr
    );

    if (GetLastError() != ERROR_MORE_DATA || needed == 0) {
        outError = "failed to enumerate services (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    std::vector<BYTE> buffer(needed);
    if (!EnumServicesStatusExW(
            scm.Get(),
            SC_ENUM_PROCESS_INFO,
            serviceType,
            serviceState,
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &needed,
            &returned,
            &resume,
            nullptr)) {
        outError = "failed to enumerate services (error " + std::to_string(GetLastError()) + ")";
        return false;
    }

    ENUM_SERVICE_STATUS_PROCESSW* entries = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
    for (DWORD i = 0; i < returned; ++i) {
        std::wstring name = entries[i].lpServiceName ? entries[i].lpServiceName : L"";
        DWORD pid = entries[i].ServiceStatusProcess.dwProcessId;
        ServiceRow row{ ToUtf8(name), pid, StateToWord(entries[i].ServiceStatusProcess.dwCurrentState) };
        outRows.push_back(row);
    }

    return true;
}
