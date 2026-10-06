#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <lmcons.h>
#include <shlobj.h>

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <mutex>
#include <algorithm>
#include <memory>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

constexpr const char* DEFAULT_RCP_PORT = "514";
constexpr size_t BUFFER_SIZE = 65536;
constexpr DWORD SOCKET_TIMEOUT_MS = 30000;

class WinsockScope {
public:
    WinsockScope() : m_initialized(false) {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
            m_initialized = true;
        }
    }

    ~WinsockScope() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized;
};

class StringUtils {
public:
    static std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    static std::wstring Utf8ToWString(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
        return wstrTo;
    }

    static fs::path Utf8ToPath(const std::string& utf8_str) {
        return fs::path(Utf8ToWString(utf8_str));
    }

    static bool IsRunningAsAdmin() {
        return IsUserAnAdmin() != FALSE;
    }

    static std::string SanitizeIncomingFilename(const std::string& raw_name) {
        fs::path p(Utf8ToWString(raw_name));
        std::string clean_name = WStringToString(p.filename().wstring());
        if (clean_name.empty() || clean_name == "." || clean_name == "..") {
            return "unnamed_file";
        }
        return clean_name;
    }

    static std::string GetCurrentLocalUser() {
        wchar_t buffer[256] = {};
        DWORD size = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
        if (GetUserNameW(buffer, &size)) {
            return WStringToString(buffer);
        }
        return "nobody";
    }
};

class FileTimestampHelper {
public:
    static FILETIME UnixTimeToFILETIME(int64_t unix_time) {
        int64_t ft_val = (unix_time * 10000000LL) + 116444736000000000LL;
        FILETIME ft;
        ft.dwLowDateTime = static_cast<DWORD>(ft_val & 0xFFFFFFFF);
        ft.dwHighDateTime = static_cast<DWORD>(ft_val >> 32);
        return ft;
    }

    static int64_t FILETIMEToUnixTime(const FILETIME& ft) {
        int64_t ft_val = (static_cast<int64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        return (ft_val - 116444736000000000LL) / 10000000LL;
    }

    static bool SetFileTimestampsWin32(const fs::path& filepath, int64_t mtime, int64_t atime) {
        HANDLE hFile = CreateFileW(filepath.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        FILETIME mft = UnixTimeToFILETIME(mtime);
        FILETIME aft = UnixTimeToFILETIME(atime);
        BOOL res = SetFileTime(hFile, NULL, &aft, &mft);
        CloseHandle(hFile);
        return res != FALSE;
    }

    static bool GetFileTimestampsWin32(const fs::path& filepath, int64_t& mtime, int64_t& atime) {
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (!GetFileAttributesExW(filepath.c_str(), GetFileExInfoStandard, &fad)) return false;
        mtime = FILETIMEToUnixTime(fad.ftLastWriteTime);
        atime = FILETIMEToUnixTime(fad.ftLastAccessTime);
        return true;
    }
};

class TempFileTracker {
public:
    static TempFileTracker& Instance() {
        static TempFileTracker instance;
        return instance;
    }

    void Register(const fs::path& p) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_activeFiles.push_back(p);
    }

    void Unregister(const fs::path& p) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find(m_activeFiles.begin(), m_activeFiles.end(), p);
        if (it != m_activeFiles.end()) {
            m_activeFiles.erase(it);
        }
    }

    void CleanupAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::error_code ec;
        for (const auto& p : m_activeFiles) {
            fs::remove(p, ec);
        }
        m_activeFiles.clear();
    }

private:
    std::mutex m_mutex;
    std::vector<fs::path> m_activeFiles;
};

struct RemotePath {
    bool is_remote = false;
    std::string user;
    std::string host;
    std::string path;

    static RemotePath Parse(const std::string& input);
};
