#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <aclapi.h>
#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <set>
#include <cwctype>
#include <iomanip>
#include <memory>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")

constexpr wchar_t VERSION[] = L"2.2.0";

enum ExitCode {
    EXIT_SUCCESS_OK      = 0,
    EXIT_PARTIAL_FAILURE = 1,
    EXIT_INVALID_ARGS    = 2,
    EXIT_NO_MATCH        = 3,
    EXIT_PRIVILEGE_ERR   = 4
};

enum SignalType {
    SIG_TERM = 15,
    SIG_KILL = 9,
    SIG_INT  = 2
};

struct ProcessEntry {
    DWORD pid = 0;
    DWORD parentPid = 0;
    std::wstring name;
    std::wstring owner;
};

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : m_h(h) {}
    ~ScopedHandle() { Close(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& o) noexcept : m_h(o.m_h) { o.m_h = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& o) noexcept {
        if (this != &o) {
            Close();
            m_h = o.m_h;
            o.m_h = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE Get() const { return m_h; }
    bool IsValid() const { return m_h != NULL && m_h != INVALID_HANDLE_VALUE; }
    operator HANDLE() const { return m_h; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_h);
            m_h = INVALID_HANDLE_VALUE;
        }
    }

    void Reset(HANDLE h = INVALID_HANDLE_VALUE) {
        Close();
        m_h = h;
    }

private:
    HANDLE m_h;
};

class PrivilegeEscalator {
public:
    static bool EnableDebugPrivilege();
};
