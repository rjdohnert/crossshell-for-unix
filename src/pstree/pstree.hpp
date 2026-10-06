#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cwctype>
#include <io.h>
#include <fcntl.h>

namespace VersionInfo {
    constexpr wchar_t VERSION[] = L"1.0.0";
    constexpr wchar_t RELEASE_DATE[] = L"2026-08-12";
    constexpr wchar_t AUTHOR[] = L"Roberto J Dohnert";
}

class ScopedHandle {
private:
    HANDLE m_handle = INVALID_HANDLE_VALUE;

public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) noexcept : m_handle(h) {}
    
    ~ScopedHandle() noexcept {
        Close();
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    void Close() noexcept {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    [[nodiscard]] HANDLE get() const noexcept { return m_handle; }
    [[nodiscard]] bool isValid() const noexcept { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }
    explicit operator bool() const noexcept { return isValid(); }
};

enum class TreeStyle {
    Unicode,    // UTF-16 Box Drawing Characters (├── └── │)
    ASCII       // Classic ASCII fallback (|-- `-- |)
};

struct Config {
    bool showPids        = false;
    bool showFullPaths   = false;
    bool numericSort     = false;
    bool colorOutput     = false;
    bool showParents     = false;
    DWORD targetPid      = 0;
    TreeStyle style      = TreeStyle::ASCII;
};

struct TreeSymbols {
    std::wstring branch;    // Item in middle of list
    std::wstring last;      // Last item in list
    std::wstring vertical;  // Vertical line for indentation
    std::wstring space;     // Empty padding
};

struct ProcessNode {
    DWORD pid = 0;
    DWORD ppid = 0;
    std::wstring name;
    std::wstring fullPath;
    std::vector<DWORD> children;
    bool isRootCandidate = false;
};

using ProcessMap = std::unordered_map<DWORD, ProcessNode>;

namespace Color {
    constexpr wchar_t RESET[]       = L"\033[0m";
    constexpr wchar_t BOLD[]        = L"\033[1m";
    constexpr wchar_t RED[]         = L"\033[31m";
    constexpr wchar_t CYAN[]        = L"\033[36m";
    constexpr wchar_t BRIGHT_RED[]  = L"\033[91m";
}
