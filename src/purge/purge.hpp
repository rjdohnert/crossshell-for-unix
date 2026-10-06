#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <fstream>
#include <chrono>
#include <sstream>
#include <random>
#include <memory>

namespace fs = std::filesystem;

constexpr uint64_t VMS_BLOCK_SIZE = 512;

class ScopedFileHandle {
public:
    explicit ScopedFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}

    ~ScopedFileHandle() {
        Close();
    }

    ScopedFileHandle(const ScopedFileHandle&) = delete;
    ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;

    ScopedFileHandle(ScopedFileHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedFileHandle& operator=(ScopedFileHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != NULL; }

    void Close() {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != NULL) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

struct FileRecord {
    fs::path fullPath;
    std::wstring baseKey;
    int64_t explicitVersion = -1;
    bool hasExplicitVersion = false;
    fs::file_time_type writeTime;
    uint64_t fileSize = 0;
};

struct PurgeMetrics {
    uint64_t totalDeleted = 0;
    uint64_t totalBytesFreed = 0;
};

class StringHelper {
public:
    static std::string ToUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return s;
    }

    static std::wstring ToUpperW(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towupper(c)); });
        return s;
    }

    static std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    static std::wstring StringToWString(const std::string& str) {
        if (str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
        return wstrTo;
    }

    static std::string FormatBytes(uint64_t bytes) {
        uint64_t blocks = (bytes + VMS_BLOCK_SIZE - 1) / VMS_BLOCK_SIZE;
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int unitIndex = 0;
        double count = static_cast<double>(bytes);

        while (count >= 1024.0 && unitIndex < 4) {
            count /= 1024.0;
            unitIndex++;
        }

        std::ostringstream ss;
        ss << blocks << " block" << (blocks == 1 ? "" : "s") << " / ";
        if (unitIndex == 0) {
            ss << static_cast<uint64_t>(count) << " " << units[unitIndex];
        } else {
            ss << std::fixed << std::setprecision(1) << count << " " << units[unitIndex];
        }
        return ss.str();
    }

    static bool WildcardMatch(const std::wstring& pattern, const std::wstring& text) {
        if (pattern.empty() || pattern == L"*") return true;
        std::wstring regStr = L"^";
        for (wchar_t c : pattern) {
            if (c == L'*') regStr += L".*";
            else if (c == L'?') regStr += L".";
            else if (wcschr(L"\\.+^$()[]{}|", c)) { regStr += L"\\"; regStr += c; }
            else regStr += c;
        }
        regStr += L"$";
        std::wregex rx(regStr, std::regex::icase);
        return std::regex_match(text, rx);
    }
};
