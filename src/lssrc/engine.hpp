#pragma once

#include "lssrc.hpp"

class ScopedServiceHandle {
public:
    explicit ScopedServiceHandle(SC_HANDLE handle = nullptr);
    ~ScopedServiceHandle();

    ScopedServiceHandle(const ScopedServiceHandle&) = delete;
    ScopedServiceHandle& operator=(const ScopedServiceHandle&) = delete;

    ScopedServiceHandle(ScopedServiceHandle&& other) noexcept;
    ScopedServiceHandle& operator=(ScopedServiceHandle&& other) noexcept;

    SC_HANDLE Get() const;
    operator SC_HANDLE() const;
    bool IsValid() const;
    void Close();
    void Reset(SC_HANDLE handle = nullptr);

private:
    SC_HANDLE m_handle;
};

class ServiceManagerEngine {
public:
    static std::wstring ToLower(std::wstring s);
    static std::string ToUtf8(const std::wstring& w);
    static const char* StateToWord(DWORD state);
    static bool EnumerateServices(bool includeStopped, std::vector<ServiceRow>& outRows, std::string& outError);
};
