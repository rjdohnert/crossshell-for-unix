#pragma once

#include "lsuser.hpp"

template <typename T>
class ScopedNetApiBuffer {
public:
    explicit ScopedNetApiBuffer(T* ptr = nullptr) : m_ptr(ptr) {}
    ~ScopedNetApiBuffer() { Free(); }

    ScopedNetApiBuffer(const ScopedNetApiBuffer&) = delete;
    ScopedNetApiBuffer& operator=(const ScopedNetApiBuffer&) = delete;

    ScopedNetApiBuffer(ScopedNetApiBuffer&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ScopedNetApiBuffer& operator=(ScopedNetApiBuffer&& other) noexcept {
        if (this != &other) {
            Free();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* Get() const { return m_ptr; }
    operator T*() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    bool IsValid() const { return m_ptr != nullptr; }

    void Free() {
        if (m_ptr) {
            NetApiBufferFree(m_ptr);
            m_ptr = nullptr;
        }
    }

private:
    T* m_ptr;
};

class UserAccountEnumerator {
public:
    static std::wstring ToWide(const std::string& input);
    static std::wstring ToLower(std::wstring value);
    static std::wstring NormalizeText(const wchar_t* value);
    static std::wstring NormalizeText(const std::wstring& value);
    static std::wstring ResolveFullName(const std::wstring& username, bool disabled);
    static std::wstring ResolveHomeDirectory(const std::wstring& username, bool disabled);
    static std::wstring ResolveDefaultShell();
    static std::wstring FormatDurationFromSeconds(ULONGLONG seconds);
    static std::wstring ResolveGroupMembership(const std::wstring& username);
    static std::wstring ResolveLoginDuration(const std::wstring& username);
    static std::vector<AccountRecord> EnumerateLocalAccounts();
};
