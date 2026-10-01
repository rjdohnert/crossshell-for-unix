/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * ============================================================================
 * SINGLE FILE INDEX
 * ============================================================================
 * 1. RAII RESOURCE GUARDS & DATA MODELS
 *    - ScopedInternetHandle, ScopedFindFileHandle, ScopedFileHandle
 *    - ScopedEnumHandle, ScopedSid, ScopedConsoleMode, ScopedOutputRedirect
 *    - RemoteLocation, TreeItem
 * 2. STRING & PATH HELPERS
 *    - StringHelper (ToWide, ToNarrow, FormatBytes, GetWeekdayName, SplitArguments)
 *    - PathHelper (GetCurrentDirectoryString, JoinPath, NormalizeSeparators,
 *                  NormalizePath, EnsureParentDirectoryExists, PathExists,
 *                  IsDirectoryPath, ResolvePathForContext, FileTimeToString)
 * 3. REMOTE PROTOCOL ENGINES
 *    - RemoteProtocolHelper (IsRemotePath, BuildRemoteUri, ParseRemoteLocation, JoinRemotePath)
 *    - FtpEngine (ListDirectory, ReadFileContent)
 *    - SshEngine (RunCommand)
 *    - RemoteInspector (ListRemoteDirectory, CatRemoteFile)
 * 4. FILE SYSTEM OPERATIONS ENGINE
 *    - SystemInspector (IsCurrentUserAdministrator, ListDrives, ListNetworkShares)
 *    - TreeRenderer (PrintTreeRecursive, RunTree)
 *    - FileOperator (ListDirectory, CatFile, StatPath, RunFind, TouchPath,
 *                    RemoveDirectoryTree, ReadTextFileLines, ShowFileDiff,
 *                    MovePathToRecycleBin, ConfirmDestructiveAction)
 * 5. COMMAND REDIRECTION & APPLICATION SHELL
 *    - RedirectionParser (Parse)
 *    - FsctlApplication (Run, command loop, dispatcher)
 *    - main entry point
 * ============================================================================
 */

#include <windows.h>
#include <aclapi.h>
#include <iomanip>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cwctype>
#include <winnetwk.h>
#include <wininet.h>
#include <shellapi.h>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <memory>

#pragma comment(lib, "mpr.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "Advapi32.lib")

// ============================================================================
// 1. RAII RESOURCE GUARDS & DATA MODELS
// ============================================================================

class ScopedInternetHandle {
public:
    explicit ScopedInternetHandle(HINTERNET handle = nullptr) : m_handle(handle) {}
    ~ScopedInternetHandle() { Close(); }

    ScopedInternetHandle(const ScopedInternetHandle&) = delete;
    ScopedInternetHandle& operator=(const ScopedInternetHandle&) = delete;

    ScopedInternetHandle(ScopedInternetHandle&& other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }
    ScopedInternetHandle& operator=(ScopedInternetHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    HINTERNET Get() const { return m_handle; }
    bool IsValid() const { return m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            InternetCloseHandle(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HINTERNET m_handle;
};

class ScopedFindFileHandle {
public:
    explicit ScopedFindFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedFindFileHandle() { Close(); }

    ScopedFindFileHandle(const ScopedFindFileHandle&) = delete;
    ScopedFindFileHandle& operator=(const ScopedFindFileHandle&) = delete;

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; }

    void Close() {
        if (IsValid()) {
            FindClose(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedFileHandle {
public:
    explicit ScopedFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedFileHandle() { Close(); }

    ScopedFileHandle(const ScopedFileHandle&) = delete;
    ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;

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

class ScopedEnumHandle {
public:
    explicit ScopedEnumHandle(HANDLE handle = nullptr) : m_handle(handle) {}
    ~ScopedEnumHandle() { Close(); }

    ScopedEnumHandle(const ScopedEnumHandle&) = delete;
    ScopedEnumHandle& operator=(const ScopedEnumHandle&) = delete;

    HANDLE Get() const { return m_handle; }
    HANDLE* AddressOf() { return &m_handle; }
    bool IsValid() const { return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (IsValid()) {
            WNetCloseEnum(m_handle);
            m_handle = nullptr;
        }
    }

private:
    HANDLE m_handle;
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

class ScopedConsoleMode {
public:
    static void EnableVirtualTerminalProcessing() {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
    }
};

class ScopedOutputRedirect {
public:
    ScopedOutputRedirect() = default;
    ~ScopedOutputRedirect() { Cleanup(); }

    bool Redirect(const std::wstring& filePath, bool append) {
        Cleanup();
        if (!append) {
            std::ofstream trunc(filePath, std::ios::trunc);
            trunc.close();
        }
        m_redirect.open(filePath, std::ios::app);

        if (m_redirect.is_open()) {
            m_oldCout = std::cout.rdbuf(m_redirect.rdbuf());
        }
        return m_redirect.is_open();
    }

    void Cleanup() {
        if (m_oldCout) {
            std::cout.rdbuf(m_oldCout);
            m_oldCout = nullptr;
        }
        if (m_redirect.is_open()) {
            m_redirect.close();
        }
    }

private:
    std::ofstream m_redirect;
    std::streambuf* m_oldCout = nullptr;
};

struct RemoteLocation {
    std::wstring scheme;
    std::wstring host;
    std::wstring user;
    std::wstring password;
    std::wstring path;
};

struct TreeItem {
    std::wstring name;
    bool isDir;
};

// ============================================================================
// 2. STRING & PATH HELPERS
// ============================================================================

class StringHelper {
public:
    static std::wstring ToWide(const std::string& input) {
        if (input.empty()) return L"";
        int needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
        if (needed <= 0) {
            needed = MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, nullptr, 0);
            if (needed <= 0) return L"";
            std::vector<wchar_t> buffer(static_cast<size_t>(needed));
            if (MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, buffer.data(), needed) <= 0) return L"";
            return std::wstring(buffer.data());
        }
        std::vector<wchar_t> buffer(static_cast<size_t>(needed));
        if (MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, buffer.data(), needed) <= 0) return L"";
        return std::wstring(buffer.data());
    }

    static std::string ToNarrow(const std::wstring& input) {
        if (input.empty()) return "";
        int needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (needed <= 0) return "";
        std::vector<char> buffer(static_cast<size_t>(needed));
        if (WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, buffer.data(), needed, nullptr, nullptr) <= 0) return "";
        return std::string(buffer.data());
    }

    static std::string GetWeekdayName(WORD dayOfWeek) {
        static const char* kWeekdays[] = {
            "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
        };
        if (dayOfWeek > 6) return "Unknown";
        return kWeekdays[dayOfWeek];
    }

    static std::wstring FormatBytes(unsigned long long bytes) {
        static const wchar_t* suffixes[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
        int suffixIndex = 0;
        double doubleBytes = static_cast<double>(bytes);
        while (doubleBytes >= 1024.0 && suffixIndex < 4) {
            doubleBytes /= 1024.0;
            suffixIndex++;
        }
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(suffixIndex == 0 ? 0 : 2) << doubleBytes << L" " << suffixes[suffixIndex];
        return ss.str();
    }

    static std::vector<std::wstring> SplitArguments(const std::wstring& input) {
        std::vector<std::wstring> args;
        std::wstring current;
        bool in_quotes = false;
        for (size_t i = 0; i < input.length(); ++i) {
            wchar_t c = input[i];
            if (c == L'"') {
                in_quotes = !in_quotes;
            } else if (c == L' ' && !in_quotes) {
                if (!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            args.push_back(current);
        }
        return args;
    }
};

class PathHelper {
public:
    static std::wstring GetCurrentDirectoryString() {
        DWORD length = GetCurrentDirectoryW(0, NULL);
        if (length == 0) return L".";
        std::wstring path(length, L'\0');
        DWORD written = GetCurrentDirectoryW(length, &path[0]);
        if (written == 0) return L".";
        path.resize(written);
        return path;
    }

    static std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
        if (base.empty()) return child;
        if (base.back() == L'\\' || base.back() == L'/') return base + child;
        return base + L"\\" + child;
    }

    static std::wstring NormalizeSeparators(const std::wstring& input) {
        std::wstring normalized = input;
        for (wchar_t& ch : normalized) {
            if (ch == L'/') ch = L'\\';
        }
        return normalized;
    }

    static bool PathExists(const std::wstring& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES;
    }

    static bool IsDirectoryPath(const std::wstring& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    static std::wstring NormalizePath(const std::wstring& base, const std::wstring& input) {
        std::wstring normalizedInput = NormalizeSeparators(input);
        bool isDriveQualified = normalizedInput.length() >= 2 && normalizedInput[1] == L':';
        bool isUncPath = normalizedInput.length() >= 2 && normalizedInput[0] == L'\\' && normalizedInput[1] == L'\\';
        bool isRootedPath = normalizedInput.length() >= 1 && (normalizedInput[0] == L'\\' || normalizedInput[0] == L'/');

        std::wstring combined = normalizedInput;
        if (!isDriveQualified && !isUncPath && !isRootedPath) {
            combined = JoinPath(base, normalizedInput);
        }

        DWORD result = GetFullPathNameW(combined.c_str(), 0, nullptr, nullptr);
        if (result == 0) return combined;

        std::wstring fullPath(result, L'\0');
        DWORD written = GetFullPathNameW(combined.c_str(), result, &fullPath[0], nullptr);
        if (written == 0 || written >= result) return combined;

        fullPath.resize(written);
        return fullPath;
    }

    static bool EnsureParentDirectoryExists(const std::wstring& path) {
        std::wstring parent = path;
        size_t slash = parent.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return true;

        parent = parent.substr(0, slash);
        if (parent.empty()) return true;

        if (PathExists(parent)) return IsDirectoryPath(parent);

        std::wstring current = parent;
        std::vector<std::wstring> parts;
        while (!PathExists(current) && !current.empty()) {
            parts.push_back(current);
            size_t split = current.find_last_of(L"\\/");
            if (split == std::wstring::npos || split == 0) break;
            current = current.substr(0, split);
        }

        while (!parts.empty()) {
            std::wstring toCreate = parts.back();
            parts.pop_back();
            if (!CreateDirectoryW(toCreate.c_str(), nullptr)) {
                DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS) return false;
            }
        }
        return true;
    }

    static std::wstring FileTimeToString(const FILETIME& ft) {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&ft, &stUTC);
        SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

        std::wstringstream ss;
        ss << std::setfill(L'0')
           << std::setw(2) << stLocal.wMonth << L"/"
           << std::setw(2) << stLocal.wDay << L"/"
           << std::setw(4) << stLocal.wYear << L" "
           << std::setw(2) << stLocal.wHour << L":"
           << std::setw(2) << stLocal.wMinute << L":"
           << std::setw(2) << stLocal.wSecond;
        return ss.str();
    }
};

// ============================================================================
// 3. REMOTE PROTOCOL ENGINES
// ============================================================================

class RemoteProtocolHelper {
public:
    static bool IsRemotePath(const std::wstring& path) {
        if (path.empty()) return false;
        std::wstring lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return std::towlower(c); });
        return lower.rfind(L"ftp://", 0) == 0 || lower.rfind(L"ssh://", 0) == 0;
    }

    static std::wstring BuildRemoteUri(const RemoteLocation& location) {
        std::wstring uri = location.scheme + L"://";
        if (!location.user.empty()) {
            uri += location.user;
            if (!location.password.empty()) {
                uri += L":" + location.password;
            }
            uri += L"@";
        }
        uri += location.host;
        if (location.path.empty()) {
            uri += L"/";
        } else {
            uri += location.path;
        }
        return uri;
    }

    static bool ParseRemoteLocation(const std::wstring& uri, RemoteLocation& location) {
        location = {};
        if (!IsRemotePath(uri)) return false;

        std::wstring lower = uri;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return std::towlower(c); });

        if (lower.rfind(L"ftp://", 0) == 0) {
            location.scheme = L"ftp";
        } else if (lower.rfind(L"ssh://", 0) == 0) {
            location.scheme = L"ssh";
        } else {
            return false;
        }

        std::wstring rest = uri.substr(location.scheme.size() + 3);
        size_t slash = rest.find(L'/');
        std::wstring authority = slash == std::wstring::npos ? rest : rest.substr(0, slash);
        std::wstring path = slash == std::wstring::npos ? L"/" : rest.substr(slash);
        if (path.empty()) path = L"/";

        size_t at = authority.rfind(L'@');
        if (at != std::wstring::npos) {
            std::wstring userinfo = authority.substr(0, at);
            location.host = authority.substr(at + 1);
            size_t colon = userinfo.find(L':');
            if (colon != std::wstring::npos) {
                location.user = userinfo.substr(0, colon);
                location.password = userinfo.substr(colon + 1);
            } else {
                location.user = userinfo;
            }
        } else {
            location.host = authority;
        }

        location.path = path;
        if (location.path.empty()) location.path = L"/";
        return true;
    }

    static std::wstring JoinRemotePath(const std::wstring& base, const std::wstring& child) {
        if (child.empty() || child == L".") return base;
        if (IsRemotePath(child)) return child;

        if (child == L"..") {
            RemoteLocation location;
            if (!ParseRemoteLocation(base, location)) return base;
            if (location.path.empty() || location.path == L"/") return base;
            size_t lastSlash = location.path.find_last_of(L"/");
            if (lastSlash == std::wstring::npos || lastSlash == 0) {
                location.path = L"/";
            } else {
                location.path = location.path.substr(0, lastSlash);
            }
            return BuildRemoteUri(location);
        }

        if (base.empty()) return child;
        if (base.back() == L'/' || base.back() == L'\\') return base + child;
        return base + L"/" + child;
    }

    static std::wstring ResolvePathForContext(const std::wstring& currentPath, const std::wstring& input) {
        if (input.empty()) return currentPath;
        if (IsRemotePath(input)) return input;
        if (IsRemotePath(currentPath)) return JoinRemotePath(currentPath, input);
        return PathHelper::NormalizePath(currentPath, input);
    }
};

class FtpEngine {
public:
    static bool ListDirectory(const RemoteLocation& location, std::vector<std::wstring>& entries) {
        ScopedInternetHandle hInternet(InternetOpenW(L"fsctl", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
        if (!hInternet.IsValid()) return false;

        ScopedInternetHandle hConnect(InternetConnectW(hInternet.Get(), location.host.c_str(), INTERNET_DEFAULT_FTP_PORT,
                                                      location.user.empty() ? L"anonymous" : location.user.c_str(),
                                                      location.password.empty() ? L"anonymous@" : location.password.c_str(),
                                                      INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0));
        if (!hConnect.IsValid()) return false;

        if (!FtpSetCurrentDirectoryW(hConnect.Get(), location.path.c_str())) return false;

        WIN32_FIND_DATAW findData;
        ScopedInternetHandle hFind(FtpFindFirstFileW(hConnect.Get(), L"*", &findData, 0, 0));
        if (!hFind.IsValid()) return false;

        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }
            entries.push_back(findData.cFileName);
        } while (InternetFindNextFileW(hFind.Get(), &findData));

        return true;
    }

    static std::string ReadFileContent(const RemoteLocation& location) {
        ScopedInternetHandle hInternet(InternetOpenW(L"fsctl", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0));
        if (!hInternet.IsValid()) return {};

        ScopedInternetHandle hConnect(InternetConnectW(hInternet.Get(), location.host.c_str(), INTERNET_DEFAULT_FTP_PORT,
                                                      location.user.empty() ? L"anonymous" : location.user.c_str(),
                                                      location.password.empty() ? L"anonymous@" : location.password.c_str(),
                                                      INTERNET_SERVICE_FTP, INTERNET_FLAG_PASSIVE, 0));
        if (!hConnect.IsValid()) return {};

        ScopedInternetHandle hFile(FtpOpenFileW(hConnect.Get(), location.path.c_str(), GENERIC_READ, FTP_TRANSFER_TYPE_BINARY, 0));
        if (!hFile.IsValid()) return {};

        std::string output;
        char buffer[4096];
        DWORD bytesRead = 0;
        while (InternetReadFile(hFile.Get(), buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            output.append(buffer, bytesRead);
        }
        return output;
    }
};

class SshEngine {
public:
    static std::wstring RunCommand(const RemoteLocation& location, const std::wstring& command) {
        std::wstring exePath;
        wchar_t resolved[MAX_PATH] = {0};

        DWORD length = SearchPathW(nullptr, L"ssh.exe", nullptr, MAX_PATH, resolved, nullptr);
        if (length > 0 && length < MAX_PATH) {
            exePath = resolved;
        } else {
            length = SearchPathW(nullptr, L"plink.exe", nullptr, MAX_PATH, resolved, nullptr);
            if (length > 0 && length < MAX_PATH) {
                exePath = resolved;
            }
        }

        if (exePath.empty()) {
            return L"SSH support requires OpenSSH (ssh.exe) or PuTTY (plink.exe) to be installed.";
        }

        std::wstring target = location.host;
        if (!location.user.empty()) {
            target = location.user + L"@" + location.host;
        }

        std::wstring cmdLine = L"\"" + exePath + L"\" -o BatchMode=yes \"" + target + L"\" \"" + command + L"\"";
        FILE* pipe = _wpopen(cmdLine.c_str(), L"r");
        if (!pipe) {
            return L"Unable to launch SSH client.";
        }

        wchar_t buffer[4096];
        std::wstring output;
        while (fgetws(buffer, _countof(buffer), pipe) != nullptr) {
            output += buffer;
        }
        _pclose(pipe);
        return output;
    }
};

class RemoteInspector {
public:
    static void ListRemoteDirectory(const std::wstring& path) {
        RemoteLocation location;
        if (!RemoteProtocolHelper::ParseRemoteLocation(path, location)) {
            std::cout << "\033[1;31mUnsupported remote location.\033[0m\n";
            return;
        }

        std::cout << "\nContents of \033[1;36m" << StringHelper::ToNarrow(path) << "\033[0m:\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(12) << "Mode"
                  << std::setw(8) << "Type"
                  << std::setw(44) << "Name"
                  << std::right << std::setw(16) << "Size\n";
        std::cout << "--------------------------------------------------------------------------------\n";

        if (location.scheme == L"ftp") {
            std::vector<std::wstring> entries;
            if (!FtpEngine::ListDirectory(location, entries)) {
                std::cout << "\033[1;31mError reading FTP directory.\033[0m\n";
                std::cout << "--------------------------------------------------------------------------------\n";
                return;
            }

            for (const auto& entry : entries) {
                std::string entryNarrow = StringHelper::ToNarrow(entry);
                int entryPad = 44 - static_cast<int>(entry.length());
                if (entryPad < 1) entryPad = 1;
                std::string padSpaces(static_cast<size_t>(entryPad), ' ');

                std::cout << std::left << std::setw(12) << "---------"
                          << std::setw(8) << "FILE"
                          << entryNarrow << padSpaces
                          << std::right << std::setw(16) << "-\n";
            }
            std::cout << "--------------------------------------------------------------------------------\n";
            return;
        }

        std::wstring output = SshEngine::RunCommand(location, L"ls -la " + location.path);
        if (output.empty()) {
            std::cout << "  <No output returned>\n";
        } else {
            std::cout << StringHelper::ToNarrow(output) << "\n";
        }
        std::cout << "--------------------------------------------------------------------------------\n";
    }

    static void CatRemoteFile(const std::wstring& path) {
        RemoteLocation location;
        if (!RemoteProtocolHelper::ParseRemoteLocation(path, location)) {
            std::cout << "\033[1;31mUnsupported remote location.\033[0m\n";
            return;
        }

        if (location.scheme == L"ftp") {
            std::string content = FtpEngine::ReadFileContent(location);
            if (content.empty()) {
                std::cout << "\033[1;31mError reading FTP file.\033[0m\n";
                return;
            }
            std::cout << content << "\n";
            return;
        }

        std::wstring output = SshEngine::RunCommand(location, L"cat " + location.path);
        std::cout << StringHelper::ToNarrow(output) << "\n";
    }
};

// ============================================================================
// 4. FILE SYSTEM OPERATIONS ENGINE
// ============================================================================

class SystemInspector {
public:
    static bool IsCurrentUserAdministrator() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

        if (AllocateAndInitializeSid(
                &ntAuthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &adminGroup)) {
            ScopedSid scopedSid(adminGroup);
            CheckTokenMembership(nullptr, scopedSid.Get(), &isAdmin);
        }

        return isAdmin == TRUE;
    }

    static void ListDrives() {
        DWORD driveMask = GetLogicalDrives();
        if (driveMask == 0) {
            std::cout << "\033[1;31mError listing drives (code " << GetLastError() << ").\033[0m\n";
            return;
        }

        std::cout << "\n\033[1;36mAvailable drives:\033[0m\n";
        std::cout << "-------------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(8) << "Drive"
                  << std::setw(12) << "Type"
                  << std::setw(12) << "Filesystem"
                  << std::setw(16) << "Total Space"
                  << std::setw(16) << "Free Space"
                  << "Label\n";
        std::cout << "-------------------------------------------------------------------------------------\n";

        for (int index = 0; index < 26; ++index) {
            if ((driveMask & (1u << index)) == 0) continue;

            wchar_t rootPath[] = L"A:\\";
            rootPath[0] = static_cast<wchar_t>(L'A' + index);

            const char* driveTypeName = "Unknown";
            switch (GetDriveTypeW(rootPath)) {
                case DRIVE_FIXED: driveTypeName = "Fixed"; break;
                case DRIVE_REMOVABLE: driveTypeName = "Removable"; break;
                case DRIVE_CDROM: driveTypeName = "CD-ROM"; break;
                case DRIVE_REMOTE: driveTypeName = "Network"; break;
                case DRIVE_RAMDISK: driveTypeName = "RAM Disk"; break;
                case DRIVE_NO_ROOT_DIR: driveTypeName = "Invalid"; break;
            }

            wchar_t volumeName[MAX_PATH] = L"";
            wchar_t fileSystemName[MAX_PATH] = L"";
            if (!GetVolumeInformationW(rootPath, volumeName, MAX_PATH, NULL, NULL,
                                       NULL, fileSystemName, MAX_PATH)) {
                volumeName[0] = L'\0';
                fileSystemName[0] = L'\0';
            }

            ULARGE_INTEGER freeBytesAvailable = {0};
            ULARGE_INTEGER totalNumberOfBytes = {0};
            ULARGE_INTEGER totalNumberOfFreeBytes = {0};
            std::wstring totalSpaceStr = L"-";
            std::wstring freeSpaceStr = L"-";

            if (GetDiskFreeSpaceExW(rootPath, &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
                totalSpaceStr = StringHelper::FormatBytes(totalNumberOfBytes.QuadPart);
                freeSpaceStr = StringHelper::FormatBytes(freeBytesAvailable.QuadPart);
            }

            std::string colorCode = "\033[0m";
            if (GetDriveTypeW(rootPath) == DRIVE_FIXED) {
                colorCode = "\033[1;32m";
            } else if (GetDriveTypeW(rootPath) == DRIVE_REMOTE || GetDriveTypeW(rootPath) == DRIVE_RAMDISK) {
                colorCode = "\033[1;36m";
            } else {
                colorCode = "\033[90m";
            }

            std::cout << colorCode << std::left << std::setw(8) << StringHelper::ToNarrow(rootPath) << "\033[0m"
                      << std::left << std::setw(12) << driveTypeName
                      << std::setw(12) << (fileSystemName[0] != L'\0' ? StringHelper::ToNarrow(fileSystemName) : "-")
                      << std::setw(16) << StringHelper::ToNarrow(totalSpaceStr)
                      << std::setw(16) << StringHelper::ToNarrow(freeSpaceStr)
                      << (volumeName[0] != L'\0' ? StringHelper::ToNarrow(volumeName) : "-")
                      << "\n";
        }

        std::cout << "-------------------------------------------------------------------------------------\n";
    }

    static void ListNetworkShares() {
        std::cout << "\n\033[1;36mAvailable Network Shares:\033[0m\n";
        std::cout << "----------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(40) << "Share Path" << "Type\n";
        std::cout << "----------------------------------------------------------------------\n";

        ScopedEnumHandle hEnum;
        NETRESOURCEW nr = {0};
        nr.dwType = RESOURCETYPE_DISK;
        nr.dwScope = RESOURCE_GLOBALNET;
        nr.dwUsage = RESOURCEUSAGE_CONNECTABLE;

        DWORD dwResult = WNetOpenEnumW(RESOURCE_GLOBALNET, RESOURCETYPE_DISK, RESOURCEUSAGE_ALL, NULL, hEnum.AddressOf());
        if (dwResult != NO_ERROR) {
            std::cout << "\033[1;31mUnable to enumerate network resources. Error: " << dwResult << "\033[0m\n";
            std::cout << "----------------------------------------------------------------------\n";
            return;
        }

        DWORD cbBuffer = 16384;
        LPNETRESOURCEW lpnrs = (LPNETRESOURCEW)GlobalAlloc(GPTR, cbBuffer);
        if (lpnrs == NULL) {
            std::cout << "\033[1;31mMemory allocation failed.\033[0m\n";
            return;
        }

        bool foundShares = false;
        do {
            DWORD cEntries = (DWORD)-1;
            dwResult = WNetEnumResourceW(hEnum.Get(), &cEntries, lpnrs, &cbBuffer);
            if (dwResult == NO_ERROR) {
                for (DWORD i = 0; i < cEntries; ++i) {
                    foundShares = true;
                    std::string remotePath = "";
                    std::string remoteType = "";

                    if (lpnrs[i].lpRemoteName) {
                        remotePath = StringHelper::ToNarrow(lpnrs[i].lpRemoteName);
                    }

                    switch (lpnrs[i].dwType) {
                        case RESOURCETYPE_DISK:
                            remoteType = "Disk";
                            break;
                        case RESOURCETYPE_PRINT:
                            remoteType = "Printer";
                            break;
                        default:
                            remoteType = "Other";
                            break;
                    }

                    std::cout << std::left << std::setw(40) << remotePath << remoteType << "\n";
                }
            } else if (dwResult != ERROR_NO_MORE_ITEMS) {
                break;
            }
        } while (dwResult == NO_ERROR);

        GlobalFree((HGLOBAL)lpnrs);

        if (!foundShares) {
            std::cout << "  <No network shares available>\n";
        }

        std::cout << "----------------------------------------------------------------------\n";
    }
};

class TreeRenderer {
public:
    static void PrintTreeRecursive(const std::wstring& baseDir, const std::wstring& indent, bool isLast,
                                   unsigned int& fileCount, unsigned int& dirCount, unsigned long long& totalSize) {
        std::wstring searchPath = PathHelper::JoinPath(baseDir, L"*");
        WIN32_FIND_DATAW findData;
        ScopedFindFileHandle findHandle(FindFirstFileW(searchPath.c_str(), &findData));
        if (!findHandle.IsValid()) return;

        std::vector<TreeItem> items;
        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }
            items.push_back({ findData.cFileName, (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 });

            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                fileCount++;
                unsigned long long fileSize = (static_cast<unsigned long long>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
                totalSize += fileSize;
            } else {
                dirCount++;
            }
        } while (FindNextFileW(findHandle.Get(), &findData) != 0);

        std::sort(items.begin(), items.end(), [](const TreeItem& a, const TreeItem& b) {
            return a.name < b.name;
        });

        for (size_t i = 0; i < items.size(); ++i) {
            bool itemIsLast = (i == items.size() - 1);
            std::string marker = itemIsLast ? "\\-- " : "|-- ";
            std::string colorPrefix = items[i].isDir ? "\033[1;36m" : "\033[0m";
            std::string colorSuffix = "\033[0m";

            std::cout << StringHelper::ToNarrow(indent) << marker << colorPrefix << StringHelper::ToNarrow(items[i].name) << colorSuffix << "\n";

            if (items[i].isDir) {
                std::wstring nextIndent = indent + (itemIsLast ? L"    " : L"|   ");
                PrintTreeRecursive(PathHelper::JoinPath(baseDir, items[i].name), nextIndent, itemIsLast, fileCount, dirCount, totalSize);
            }
        }
    }

    static void RunTree(const std::wstring& baseDir) {
        std::cout << "\033[1;36m" << StringHelper::ToNarrow(baseDir) << "\033[0m\n";
        unsigned int fileCount = 0;
        unsigned int dirCount = 0;
        unsigned long long totalSize = 0;

        PrintTreeRecursive(baseDir, L"", true, fileCount, dirCount, totalSize);

        std::cout << "----------------------------------------------------------------------\n";
        std::cout << "Summary: " << fileCount << " File(s), " << dirCount << " Dir(s), Total size: " << StringHelper::ToNarrow(StringHelper::FormatBytes(totalSize)) << "\n";
        std::cout << "----------------------------------------------------------------------\n";
    }
};

class FileOperator {
public:
    static std::wstring GetWindowsModeString(DWORD attributes, const std::wstring& fullPath = L"") {
        std::wstring mode = L"---------";
        if (attributes & FILE_ATTRIBUTE_DIRECTORY)     mode[0] = L'd';
        if (attributes & FILE_ATTRIBUTE_READONLY)      mode[1] = L'r';
        if (attributes & FILE_ATTRIBUTE_ARCHIVE)       mode[2] = L'a';
        if (attributes & FILE_ATTRIBUTE_HIDDEN)        mode[3] = L'h';
        if (attributes & FILE_ATTRIBUTE_SYSTEM)        mode[4] = L's';
        if (attributes & FILE_ATTRIBUTE_COMPRESSED)    mode[5] = L'c';
        if (attributes & FILE_ATTRIBUTE_ENCRYPTED)     mode[6] = L'e';
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) mode[7] = L'l';

        if (!fullPath.empty()) {
            PSECURITY_DESCRIPTOR pSD = nullptr;
            PACL pDacl = nullptr;
            if (GetNamedSecurityInfoW(fullPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                     nullptr, nullptr, &pDacl, nullptr, &pSD) == ERROR_SUCCESS) {
                if (pDacl != nullptr && pDacl->AceCount > 0) {
                    mode[8] = L'+';
                }
                if (pSD) LocalFree(pSD);
            }
        }
        return mode;
    }

    static void ClassifyFile(const std::wstring& wname, bool isDir, bool& isExecutable, bool& isSource, bool& isImage) {
        isExecutable = false;
        isSource = false;
        isImage = false;
        if (isDir) return;

        size_t dot = wname.find_last_of(L'.');
        if (dot == std::wstring::npos) return;

        std::wstring ext = wname.substr(dot);
        for (auto& ch : ext) ch = std::towlower(ch);

        if (ext == L".exe" || ext == L".bat" || ext == L".cmd" || ext == L".ps1" || ext == L".com" || ext == L".msi" || ext == L".vbs") {
            isExecutable = true;
        } else if (ext == L".c" || ext == L".cpp" || ext == L".cxx" || ext == L".cc" || ext == L".h" || ext == L".hpp" ||
                   ext == L".cs" || ext == L".rs" || ext == L".go" || ext == L".py" || ext == L".java" || ext == L".js" ||
                   ext == L".ts" || ext == L".html" || ext == L".css" || ext == L".sql" || ext == L".sh" || ext == L".asm" ||
                   ext == L".json" || ext == L".xml" || ext == L".yaml" || ext == L".yml" || ext == L".toml" || ext == L".lua" ||
                   ext == L".spec" || ext == L".md" || ext == L".php" || ext == L".rb" || ext == L".swift" || ext == L".kt" ||
                   ext == L".dart" || ext == L".r" || ext == L".pl" || ext == L".cmake" || ext == L".txt") {
            isSource = true;
        } else if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".gif" || ext == L".bmp" || ext == L".svg" ||
                   ext == L".webp" || ext == L".ico" || ext == L".tiff" || ext == L".tif" || ext == L".psd" || ext == L".raw" ||
                   ext == L".heic" || ext == L".avif") {
            isImage = true;
        }
    }

    static void ListDirectory(const std::wstring& path, bool showHidden = false) {
        if (RemoteProtocolHelper::IsRemotePath(path)) {
            RemoteInspector::ListRemoteDirectory(path);
            return;
        }

        std::cout << "\nContents of \033[1;36m" << StringHelper::ToNarrow(path) << "\033[0m:\n";
        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(12) << "Mode"
                  << std::setw(8) << "Type"
                  << std::setw(44) << "Name"
                  << std::right << std::setw(16) << "Size\n";
        std::cout << "--------------------------------------------------------------------------------\n";

        WIN32_FIND_DATAW findData;
        ScopedFindFileHandle findHandle(FindFirstFileW(PathHelper::JoinPath(path, L"*").c_str(), &findData));
        if (!findHandle.IsValid()) {
            std::cout << "\033[1;31mError reading directory.\033[0m\n";
            std::cout << "--------------------------------------------------------------------------------\n";
            return;
        }

        size_t fileCount = 0;
        size_t dirCount = 0;
        unsigned long long totalFilesSize = 0;
        bool foundAny = false;

        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }

            std::wstring wname = findData.cFileName;
            bool isHidden = (findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0 || (!wname.empty() && wname[0] == L'.');
            if (isHidden && !showHidden) {
                continue;
            }

            foundAny = true;
            std::string name = StringHelper::ToNarrow(wname);
            std::wstring fullPath = PathHelper::JoinPath(path, wname);
            std::string modeStr = StringHelper::ToNarrow(GetWindowsModeString(findData.dwFileAttributes, fullPath));

            int namePad = 44 - static_cast<int>(wname.length());
            if (namePad < 1) namePad = 1;
            std::string padSpaces(static_cast<size_t>(namePad), ' ');

            bool isDir = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool isExecutable = false, isSource = false, isImage = false;
            ClassifyFile(wname, isDir, isExecutable, isSource, isImage);

            if (isDir) {
                dirCount++;
                std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                          << "\033[1;36m[DIR]   \033[0m"
                          << "\033[1;36m" << name << "\033[0m" << padSpaces
                          << std::right << std::setw(16) << "--\n";
            } else {
                fileCount++;
                unsigned long long size = (static_cast<unsigned long long>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
                totalFilesSize += size;
                std::string sizeText = StringHelper::ToNarrow(StringHelper::FormatBytes(size));

                if (isExecutable) {
                    std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                              << "\033[1;32m[EXE]   \033[0m"
                              << "\033[1;32m" << name << "\033[0m" << padSpaces
                              << std::right << std::setw(16) << sizeText << "\n";
                } else if (isSource) {
                    std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                              << "\033[38;2;255;140;0m[SRC]   \033[0m"
                              << "\033[38;2;255;140;0m" << name << "\033[0m" << padSpaces
                              << std::right << std::setw(16) << sizeText << "\n";
                } else if (isImage) {
                    std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                              << "\033[38;2;255;105;180m[IMG]   \033[0m"
                              << "\033[38;2;255;105;180m" << name << "\033[0m" << padSpaces
                              << std::right << std::setw(16) << sizeText << "\n";
                } else {
                    std::cout << "\033[90m" << std::left << std::setw(12) << modeStr << "\033[0m"
                              << "FILE    "
                              << name << padSpaces
                              << std::right << std::setw(16) << sizeText << "\n";
                }
            }
        } while (FindNextFileW(findHandle.Get(), &findData) != 0);

        if (!foundAny) {
            std::cout << "  <Directory is empty>\n";
        }

        std::cout << "--------------------------------------------------------------------------------\n";
        std::cout << "Summary: " << fileCount << " File(s), " << dirCount << " Dir(s), Total size: " << StringHelper::ToNarrow(StringHelper::FormatBytes(totalFilesSize)) << "\n";
        std::cout << "--------------------------------------------------------------------------------\n";
    }

    static void CatFile(const std::wstring& path) {
        if (RemoteProtocolHelper::IsRemotePath(path)) {
            RemoteInspector::CatRemoteFile(path);
            return;
        }

        ScopedFileHandle hFile(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
        if (!hFile.IsValid()) {
            std::cout << "\033[1;31mError opening file: " << StringHelper::ToNarrow(path) << " (code " << GetLastError() << ")\033[0m\n";
            return;
        }

        char buffer[4096];
        DWORD bytesRead = 0;
        while (ReadFile(hFile.Get(), buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::cout << buffer;
        }
        std::cout << "\n";
    }

    static void StatPath(const std::wstring& path) {
        WIN32_FILE_ATTRIBUTE_DATA attrData;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrData)) {
            std::cout << "\033[1;31mError getting attributes for: " << StringHelper::ToNarrow(path) << " (code " << GetLastError() << ")\033[0m\n";
            return;
        }

        std::cout << "\n\033[1;36mFile Details:\033[0m " << StringHelper::ToNarrow(path) << "\n";
        std::cout << "-----------------------------------------------------------------\n";

        bool isDir = (attrData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        std::cout << "Type:             " << (isDir ? "\033[1;36mDirectory\033[0m" : "File") << "\n";
        std::cout << "Mode:             " << StringHelper::ToNarrow(GetWindowsModeString(attrData.dwFileAttributes, path)) << " (Windows Native)\n";

        unsigned long long size = (static_cast<unsigned long long>(attrData.nFileSizeHigh) << 32) | attrData.nFileSizeLow;
        std::cout << "Size:             " << StringHelper::ToNarrow(StringHelper::FormatBytes(size)) << " (" << size << " bytes)\n";

        std::cout << "Created:          " << StringHelper::ToNarrow(PathHelper::FileTimeToString(attrData.ftCreationTime)) << "\n";
        std::cout << "Last Access:      " << StringHelper::ToNarrow(PathHelper::FileTimeToString(attrData.ftLastAccessTime)) << "\n";
        std::cout << "Last Modify:      " << StringHelper::ToNarrow(PathHelper::FileTimeToString(attrData.ftLastWriteTime)) << "\n";

        std::cout << "Attributes:       ";
        std::vector<std::string> attrs;
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_READONLY) attrs.push_back("Read-Only");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) attrs.push_back("Hidden");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) attrs.push_back("System");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) attrs.push_back("Directory");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) attrs.push_back("Archive");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED) attrs.push_back("Compressed");
        if (attrData.dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED) attrs.push_back("Encrypted");

        for (size_t i = 0; i < attrs.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << attrs[i];
        }
        if (attrs.empty()) std::cout << "None";
        std::cout << "\n";
        std::cout << "-----------------------------------------------------------------\n";
    }

    static void FindFilesRecursive(const std::wstring& baseDir, const std::wstring& pattern, std::vector<std::wstring>& results) {
        WIN32_FIND_DATAW findData;
        std::wstring searchPath = PathHelper::JoinPath(baseDir, L"*");
        ScopedFindFileHandle findHandle(FindFirstFileW(searchPath.c_str(), &findData));
        if (!findHandle.IsValid()) return;

        do {
            if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
                continue;
            }

            std::wstring childName = findData.cFileName;
            std::wstring fullPath = PathHelper::JoinPath(baseDir, childName);

            std::wstring lowerChild = childName;
            std::wstring lowerPattern = pattern;
            for (auto& c : lowerChild) c = std::towlower(c);
            for (auto& c : lowerPattern) c = std::towlower(c);

            if (lowerChild.find(lowerPattern) != std::wstring::npos) {
                results.push_back(fullPath);
            }

            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                FindFilesRecursive(fullPath, pattern, results);
            }
        } while (FindNextFileW(findHandle.Get(), &findData) != 0);
    }

    static void RunFind(const std::wstring& baseDir, const std::wstring& pattern) {
        std::cout << "\nSearching for \"" << StringHelper::ToNarrow(pattern) << "\" starting from " << StringHelper::ToNarrow(baseDir) << "...\n";
        std::vector<std::wstring> results;
        FindFilesRecursive(baseDir, pattern, results);

        if (results.empty()) {
            std::cout << "\033[1;33mNo matches found.\033[0m\n";
            return;
        }

        std::cout << "\033[1;32mFound " << results.size() << " match(es):\033[0m\n";
        for (const auto& path : results) {
            if (PathHelper::IsDirectoryPath(path)) {
                std::cout << "  \033[1;36m" << StringHelper::ToNarrow(path) << "/\033[0m\n";
            } else {
                std::cout << "  " << StringHelper::ToNarrow(path) << "\n";
            }
        }
    }

    static bool ConfirmDestructiveAction(const std::wstring& actionDescription) {
        std::cout << "\033[1;33mConfirm " << StringHelper::ToNarrow(actionDescription) << "? [y/N]: \033[0m";
        std::string response;
        if (!std::getline(std::cin, response)) {
            return false;
        }
        std::wstring wresponse = StringHelper::ToWide(response);
        std::transform(wresponse.begin(), wresponse.end(), wresponse.begin(), [](wchar_t c) {
            return std::towlower(c);
        });
        return wresponse == L"y" || wresponse == L"yes";
    }

    static void TouchPath(const std::wstring& path, bool noCreate) {
        if (RemoteProtocolHelper::IsRemotePath(path)) {
            std::cout << "\033[1;31mError: touch is supported for local paths only.\033[0m\n";
            return;
        }

        bool existed = PathHelper::PathExists(path);
        if (existed && PathHelper::IsDirectoryPath(path)) {
            std::cout << "\033[1;31mError: Target is a directory: " << StringHelper::ToNarrow(path) << "\033[0m\n";
            return;
        }

        if (!existed && noCreate) {
            std::cout << "\033[90mSkipped (missing): " << StringHelper::ToNarrow(path) << "\033[0m\n";
            return;
        }

        if (!existed && !PathHelper::EnsureParentDirectoryExists(path)) {
            std::cout << "\033[1;31mError: Unable to create parent directory for: " << StringHelper::ToNarrow(path) << "\033[0m\n";
            return;
        }

        ScopedFileHandle hFile(CreateFileW(
            path.c_str(),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        ));

        if (!hFile.IsValid()) {
            std::cout << "\033[1;31mError touching file (code " << GetLastError() << ").\033[0m\n";
            return;
        }

        FILETIME now;
        GetSystemTimeAsFileTime(&now);
        if (!SetFileTime(hFile.Get(), nullptr, nullptr, &now)) {
            DWORD err = GetLastError();
            std::cout << "\033[1;31mError updating timestamp (code " << err << ").\033[0m\n";
            return;
        }

        std::cout << "\033[1;32m" << (existed ? "Timestamp updated: " : "File created: ") << StringHelper::ToNarrow(path) << "\033[0m\n";
    }

    static bool RemoveDirectoryTree(const std::wstring& path, uintmax_t& removedCount) {
        std::error_code ec;
        removedCount = std::filesystem::remove_all(path, ec);
        return !ec;
    }

    static bool ReadTextFileLines(const std::wstring& path, std::vector<std::string>& lines, std::string& error) {
        lines.clear();
        error.clear();

        std::ifstream file{ std::filesystem::path(path) };
        if (!file.is_open()) {
            error = "Unable to open file.";
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }

        if (file.bad()) {
            error = "I/O error while reading file.";
            return false;
        }

        return true;
    }

    static void ShowFileDiff(const std::wstring& leftPath, const std::wstring& rightPath) {
        if (RemoteProtocolHelper::IsRemotePath(leftPath) || RemoteProtocolHelper::IsRemotePath(rightPath)) {
            std::cout << "\033[1;31mError: diff is supported for local files only.\033[0m\n";
            return;
        }

        if (!PathHelper::PathExists(leftPath) || PathHelper::IsDirectoryPath(leftPath)) {
            std::cout << "\033[1;31mError: Left file is invalid: " << StringHelper::ToNarrow(leftPath) << "\033[0m\n";
            return;
        }
        if (!PathHelper::PathExists(rightPath) || PathHelper::IsDirectoryPath(rightPath)) {
            std::cout << "\033[1;31mError: Right file is invalid: " << StringHelper::ToNarrow(rightPath) << "\033[0m\n";
            return;
        }

        std::vector<std::string> leftLines;
        std::vector<std::string> rightLines;
        std::string error;
        if (!ReadTextFileLines(leftPath, leftLines, error)) {
            std::cout << "\033[1;31mError reading left file: " << error << "\033[0m\n";
            return;
        }
        if (!ReadTextFileLines(rightPath, rightLines, error)) {
            std::cout << "\033[1;31mError reading right file: " << error << "\033[0m\n";
            return;
        }

        size_t maxLines = (std::max)(leftLines.size(), rightLines.size());
        size_t diffCount = 0;
        for (size_t i = 0; i < maxLines; ++i) {
            const std::string left = (i < leftLines.size()) ? leftLines[i] : std::string();
            const std::string right = (i < rightLines.size()) ? rightLines[i] : std::string();
            if (left != right) {
                ++diffCount;
                std::cout << "Line " << (i + 1) << "\n";
                std::cout << "< " << left << "\n";
                std::cout << "> " << right << "\n";
            }
        }

        if (diffCount == 0) {
            std::cout << "\033[1;32mFiles are identical.\033[0m\n";
        } else {
            std::cout << "\033[1;33mDiff complete: " << diffCount << " differing line(s).\033[0m\n";
        }
    }

    static bool MovePathToRecycleBin(const std::wstring& path) {
        std::wstring from = path;
        from.push_back(L'\0'); // double-null terminated string list for SHFileOperation

        SHFILEOPSTRUCTW fileOp = {};
        fileOp.wFunc = FO_DELETE;
        fileOp.pFrom = from.c_str();
        fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

        int result = SHFileOperationW(&fileOp);
        return result == 0 && !fileOp.fAnyOperationsAborted;
    }
};

// ============================================================================
// 5. COMMAND REDIRECTION & APPLICATION SHELL
// ============================================================================

class RedirectionParser {
public:
    static bool Parse(std::wstring& input, std::wstring& redirectFile, bool& append) {
        redirectFile.clear();
        append = false;

        bool in_quotes = false;
        size_t redirectPos = std::wstring::npos;

        for (size_t i = 0; i < input.length(); ++i) {
            wchar_t c = input[i];
            if (c == L'"') {
                in_quotes = !in_quotes;
            } else if (c == L'>' && !in_quotes) {
                redirectPos = i;
                break;
            }
        }

        if (redirectPos == std::wstring::npos) {
            return false;
        }

        if (redirectPos + 1 < input.length() && input[redirectPos + 1] == L'>') {
            append = true;
            std::wstring filePart = input.substr(redirectPos + 2);
            input = input.substr(0, redirectPos);
            std::vector<std::wstring> fileArgs = StringHelper::SplitArguments(filePart);
            if (!fileArgs.empty()) {
                redirectFile = fileArgs[0];
            }
        } else {
            append = false;
            std::wstring filePart = input.substr(redirectPos + 1);
            input = input.substr(0, redirectPos);
            std::vector<std::wstring> fileArgs = StringHelper::SplitArguments(filePart);
            if (!fileArgs.empty()) {
                redirectFile = fileArgs[0];
            }
        }

        return !redirectFile.empty();
    }
};

class FsctlApplication {
public:
    int Run() {
        ScopedConsoleMode::EnableVirtualTerminalProcessing();
        std::wstring current_path = PathHelper::GetCurrentDirectoryString();
        std::string input;
        std::vector<std::string> commandHistory;
        bool confirmDestructive = true;
        const bool isAdminUser = SystemInspector::IsCurrentUserAdministrator();
        SYSTEMTIME localTime;
        GetLocalTime(&localTime);

        std::cout << "\n\n";
        std::cout << "                       File System Control Shell v7.32\n\n";
        std::cout << "Copyright (C) 2026, Roberto J. Dohnert\n";
        std::cout << "All rights reserved.\n\n";
        std::cout << "Type 'help' to see the available commands.\n\n";
        std::cout << "Date: "
                  << std::setfill('0') << std::setw(2) << localTime.wMonth << "/"
                  << std::setw(2) << localTime.wDay << "/"
                  << std::setw(4) << localTime.wYear
                  << "  Time: "
                  << std::setw(2) << localTime.wHour << ":"
                  << std::setw(2) << localTime.wMinute << ":"
                  << std::setw(2) << localTime.wSecond
                  << "  Day: " << StringHelper::GetWeekdayName(localTime.wDayOfWeek) << "\n";
        std::cout << std::setfill(' ');

        while (true) {
            std::cout << "\n\033[1;36m[fsctl]";
            if (RemoteProtocolHelper::IsRemotePath(current_path)) {
                std::cout << "(remote)";
            }
            if (!confirmDestructive) {
                std::cout << "(confirm: off)";
            }
            std::cout << "\033[0m " << (isAdminUser ? "# " : ">> ");
            if (!std::getline(std::cin, input)) {
                break;
            }

            if (!input.empty()) {
                commandHistory.push_back(input);
                if (commandHistory.size() > 500) {
                    commandHistory.erase(commandHistory.begin());
                }
            }

            std::wstring winput = StringHelper::ToWide(input);
            std::wstring redirectFile;
            bool append = false;
            ScopedOutputRedirect redirectGuard;

            if (RedirectionParser::Parse(winput, redirectFile, append)) {
                std::wstring redirectPath = PathHelper::NormalizePath(current_path, redirectFile);
                if (!PathHelper::EnsureParentDirectoryExists(redirectPath)) {
                    std::cout << "\033[1;31mError: Unable to create parent directory for redirect target.\033[0m\n";
                    continue;
                }
                redirectGuard.Redirect(redirectPath, append);
            }

            std::vector<std::wstring> wargs = StringHelper::SplitArguments(winput);
            if (wargs.empty()) {
                continue;
            }

            std::string command = StringHelper::ToNarrow(wargs[0]);

            if (command == "exit" || command == "quit") {
                break;
            }
            else if (command == "drive") {
                SystemInspector::ListDrives();
            }
            else if (command == "shares" || command == "net") {
                SystemInspector::ListNetworkShares();
            }
            else if (command == "ls" || command == "dir") {
                bool isTree = false;
                bool showHidden = false;
                std::wstring targetPath = current_path;
                bool targetSpecified = false;
                bool invalidOption = false;

                for (size_t i = 1; i < wargs.size(); ++i) {
                    if (wargs[i] == L"-t" || wargs[i] == L"--tree") {
                        isTree = true;
                    } else if (wargs[i] == L"-h" || wargs[i] == L"--hidden" || wargs[i] == L"-a" || wargs[i] == L"--all") {
                        showHidden = true;
                    } else if (!wargs[i].empty() && wargs[i][0] == L'-') {
                        std::cout << "\033[1;31mError: Unknown " << command << " option: " << StringHelper::ToNarrow(wargs[i]) << "\033[0m\n";
                        std::cout << "Usage: " << command << " [-h|--hidden] [-t|--tree] [path]\n";
                        invalidOption = true;
                        break;
                    } else {
                        if (!targetSpecified) {
                            targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[i]);
                            targetSpecified = true;
                        }
                    }
                }

                if (invalidOption) {
                    continue;
                }

                if (isTree) {
                    if (RemoteProtocolHelper::IsRemotePath(targetPath) || (PathHelper::PathExists(targetPath) && PathHelper::IsDirectoryPath(targetPath))) {
                        TreeRenderer::RunTree(targetPath);
                    } else {
                        std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(targetPath) << "\033[0m\n";
                    }
                } else {
                    if (RemoteProtocolHelper::IsRemotePath(targetPath) || (PathHelper::PathExists(targetPath) && PathHelper::IsDirectoryPath(targetPath))) {
                        FileOperator::ListDirectory(targetPath, showHidden);
                    } else {
                        std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(targetPath) << "\033[0m\n";
                    }
                }
            }
            else if (command == "tree") {
                std::wstring targetPath = current_path;
                if (wargs.size() > 1) {
                    targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[1]);
                }
                if (RemoteProtocolHelper::IsRemotePath(targetPath) || (PathHelper::PathExists(targetPath) && PathHelper::IsDirectoryPath(targetPath))) {
                    TreeRenderer::RunTree(targetPath);
                } else {
                    std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(wargs.size() > 1 ? wargs[1] : L".") << "\033[0m\n";
                }
            }
            else if (command == "cd") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Directory path required.\033[0m\n";
                    continue;
                }

                std::wstring target_path = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[1]);

                if (RemoteProtocolHelper::IsRemotePath(target_path)) {
                    current_path = target_path;
                } else if (PathHelper::PathExists(target_path) && PathHelper::IsDirectoryPath(target_path)) {
                    current_path = target_path;
                    SetCurrentDirectoryW(current_path.c_str());
                } else {
                    std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                }
            }
            else if (command == "pwd") {
                std::cout << StringHelper::ToNarrow(current_path) << "\n";
            }
            else if (command == "mkdir") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Directory name required.\033[0m\n";
                    continue;
                }

                std::wstring newDir = PathHelper::NormalizePath(current_path, wargs[1]);
                if (PathHelper::PathExists(newDir)) {
                    std::cout << "\033[1;31mError: Path already exists: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (!PathHelper::EnsureParentDirectoryExists(newDir)) {
                    std::cout << "\033[1;31mError: Unable to create parent directories for: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (CreateDirectoryW(newDir.c_str(), NULL) != 0) {
                    std::cout << "\033[1;32mDirectory created successfully: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else {
                    DWORD err = GetLastError();
                    if (err == ERROR_ALREADY_EXISTS) {
                        std::cout << "\033[1;31mError: Directory already exists or creation failed.\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError creating directory (code " << err << ").\033[0m\n";
                    }
                }
            }
            else if (command == "rmdir") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Directory name required.\033[0m\n";
                    continue;
                }

                std::wstring targetDir = PathHelper::NormalizePath(current_path, wargs[1]);
                if (!PathHelper::PathExists(targetDir)) {
                    std::cout << "\033[1;31mError: Directory does not exist: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (!PathHelper::IsDirectoryPath(targetDir)) {
                    std::cout << "\033[1;31mError: Target is not a directory: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (confirmDestructive && !FileOperator::ConfirmDestructiveAction(L"deletion of directory '" + targetDir + L"'")) {
                    std::cout << "\033[90mCancelled.\033[0m\n";
                } else if (RemoveDirectoryW(targetDir.c_str()) != 0) {
                    std::cout << "\033[1;32mDirectory removed successfully: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else {
                    std::cout << "\033[1;31mError removing directory (code " << GetLastError() << ").\033[0m\n";
                }
            }
            else if (command == "trash") {
                bool force = false;
                bool invalidOption = false;
                std::vector<std::wstring> targets;

                for (size_t i = 1; i < wargs.size(); ++i) {
                    if (wargs[i] == L"-f" || wargs[i] == L"--force") {
                        force = true;
                    } else if (!wargs[i].empty() && wargs[i][0] == L'-') {
                        std::cout << "\033[1;31mError: Unknown trash option: " << StringHelper::ToNarrow(wargs[i]) << "\033[0m\n";
                        std::cout << "Usage: trash [-f|--force] <path> [path2 ...]\n";
                        invalidOption = true;
                        break;
                    } else {
                        targets.push_back(wargs[i]);
                    }
                }

                if (invalidOption) {
                    continue;
                }

                if (targets.empty()) {
                    std::cout << "Usage: trash [-f|--force] <path> [path2 ...]\n";
                    continue;
                }

                for (const auto& target : targets) {
                    std::wstring targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, target);
                    if (RemoteProtocolHelper::IsRemotePath(targetPath)) {
                        std::cout << "\033[1;31mError: trash is supported for local paths only.\033[0m\n";
                        continue;
                    }
                    if (!PathHelper::PathExists(targetPath)) {
                        if (force) {
                            std::cout << "\033[90mSkipped (not found): " << StringHelper::ToNarrow(target) << "\033[0m\n";
                        } else {
                            std::cout << "\033[1;31mError: Path does not exist: " << StringHelper::ToNarrow(target) << "\033[0m\n";
                        }
                        continue;
                    }

                    if (!force && confirmDestructive && !FileOperator::ConfirmDestructiveAction(L"move to recycle bin '" + targetPath + L"'")) {
                        std::cout << "\033[90mCancelled.\033[0m\n";
                        continue;
                    }

                    if (FileOperator::MovePathToRecycleBin(targetPath)) {
                        std::cout << "\033[1;32mMoved to recycle bin: " << StringHelper::ToNarrow(target) << "\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError moving to recycle bin: " << StringHelper::ToNarrow(target) << "\033[0m\n";
                    }
                }
            }
            else if (command == "rm") {
                bool recursive = false;
                bool force = false;
                bool invalidOption = false;
                std::vector<std::wstring> positional;

                for (size_t i = 1; i < wargs.size(); ++i) {
                    if (wargs[i] == L"-r" || wargs[i] == L"--recursive") {
                        recursive = true;
                    } else if (wargs[i] == L"-f" || wargs[i] == L"--force") {
                        force = true;
                    } else if (!wargs[i].empty() && wargs[i][0] == L'-') {
                        std::cout << "\033[1;31mError: Unknown rm option: " << StringHelper::ToNarrow(wargs[i]) << "\033[0m\n";
                        std::cout << "Usage: rm [-r|--recursive] [-f|--force] <path>\n";
                        invalidOption = true;
                        break;
                    } else {
                        positional.push_back(wargs[i]);
                    }
                }

                if (invalidOption) {
                    continue;
                }

                if (positional.size() != 1) {
                    std::cout << "Usage: rm [-r|--recursive] [-f|--force] <path>\n";
                    continue;
                }

                std::wstring targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, positional[0]);
                if (RemoteProtocolHelper::IsRemotePath(targetPath)) {
                    std::cout << "\033[1;31mError: rm is supported for local paths only.\033[0m\n";
                    continue;
                }

                if (!PathHelper::PathExists(targetPath)) {
                    if (force) {
                        std::cout << "\033[90mSkipped (not found): " << StringHelper::ToNarrow(positional[0]) << "\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError: Path does not exist: " << StringHelper::ToNarrow(positional[0]) << "\033[0m\n";
                    }
                    continue;
                }

                if (PathHelper::IsDirectoryPath(targetPath)) {
                    if (!recursive) {
                        std::cout << "\033[1;31mError: Target is a directory. Use rm -r or rmdir.\033[0m\n";
                        std::cout << "Usage: rm [-r|--recursive] [-f|--force] <path>\n";
                        continue;
                    }

                    if (!force && confirmDestructive && !FileOperator::ConfirmDestructiveAction(L"recursive delete of directory '" + targetPath + L"'")) {
                        std::cout << "\033[90mCancelled.\033[0m\n";
                        continue;
                    }

                    uintmax_t removedCount = 0;
                    if (FileOperator::RemoveDirectoryTree(targetPath, removedCount)) {
                        std::cout << "\033[1;32mRemoved directory tree: " << StringHelper::ToNarrow(positional[0])
                                  << " (" << removedCount << " item(s))\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError removing directory tree.\033[0m\n";
                    }
                } else {
                    if (!force && confirmDestructive && !FileOperator::ConfirmDestructiveAction(L"delete file '" + targetPath + L"'")) {
                        std::cout << "\033[90mCancelled.\033[0m\n";
                        continue;
                    }

                    if (DeleteFileW(targetPath.c_str()) != 0) {
                        std::cout << "\033[1;32mFile removed successfully: " << StringHelper::ToNarrow(positional[0]) << "\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError removing file (code " << GetLastError() << ").\033[0m\n";
                    }
                }
            }
            else if (command == "touch") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: At least one file path is required.\033[0m\n";
                    std::cout << "Usage: touch [-c|--no-create] <file> [file2 ...]\n";
                    continue;
                }

                bool noCreate = false;
                bool invalidOption = false;
                std::vector<std::wstring> touchTargets;

                for (size_t i = 1; i < wargs.size(); ++i) {
                    if (wargs[i] == L"-c" || wargs[i] == L"--no-create") {
                        noCreate = true;
                    } else if (!wargs[i].empty() && wargs[i][0] == L'-') {
                        std::cout << "\033[1;31mError: Unknown touch option: " << StringHelper::ToNarrow(wargs[i]) << "\033[0m\n";
                        std::cout << "Usage: touch [-c|--no-create] <file> [file2 ...]\n";
                        invalidOption = true;
                        break;
                    } else {
                        touchTargets.push_back(wargs[i]);
                    }
                }

                if (invalidOption) {
                    continue;
                }

                if (touchTargets.empty()) {
                    std::cout << "\033[1;31mError: At least one file path is required.\033[0m\n";
                    std::cout << "Usage: touch [-c|--no-create] <file> [file2 ...]\n";
                    continue;
                }

                for (const auto& target : touchTargets) {
                    std::wstring targetPath = RemoteProtocolHelper::ResolvePathForContext(current_path, target);
                    FileOperator::TouchPath(targetPath, noCreate);
                }
            }
            else if (command == "copy") {
                if (wargs.size() < 3) {
                    std::cout << "Usage: copy <source_file> <destination_file>\n";
                    continue;
                }

                std::wstring sourcePath = PathHelper::NormalizePath(current_path, wargs[1]);
                std::wstring destinationPath = PathHelper::NormalizePath(current_path, wargs[2]);

                if (PathHelper::PathExists(destinationPath) && PathHelper::IsDirectoryPath(destinationPath)) {
                    std::cout << "\033[1;31mError: Destination path is a directory.\033[0m\n";
                } else if (!PathHelper::PathExists(sourcePath)) {
                    std::cout << "\033[1;31mError: Source file does not exist: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (PathHelper::IsDirectoryPath(sourcePath)) {
                    std::cout << "\033[1;31mError: Source path is a directory. copy supports files only.\033[0m\n";
                } else if (CopyFileW(sourcePath.c_str(), destinationPath.c_str(), TRUE) != 0) {
                    std::cout << "\033[1;32mFile copied successfully.\033[0m\n";
                } else {
                    DWORD err = GetLastError();
                    if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS) {
                        std::cout << "\033[1;31mError: Destination file already exists.\033[0m\n";
                    } else {
                        std::cout << "\033[1;31mError copying file (code " << err << ").\033[0m\n";
                    }
                }
            }
            else if (command == "move") {
                if (wargs.size() < 3) {
                    std::cout << "Usage: move <source_file> <destination_file>\n";
                    continue;
                }

                std::wstring sourcePath = PathHelper::NormalizePath(current_path, wargs[1]);
                std::wstring destinationPath = PathHelper::NormalizePath(current_path, wargs[2]);

                if (PathHelper::PathExists(destinationPath) && PathHelper::IsDirectoryPath(destinationPath)) {
                    std::cout << "\033[1;31mError: Destination path is a directory.\033[0m\n";
                } else if (!PathHelper::PathExists(sourcePath)) {
                    std::cout << "\033[1;31mError: Source file does not exist: " << StringHelper::ToNarrow(wargs[1]) << "\033[0m\n";
                } else if (PathHelper::IsDirectoryPath(sourcePath)) {
                    std::cout << "\033[1;31mError: Source path is a directory. move supports files only.\033[0m\n";
                } else if (MoveFileW(sourcePath.c_str(), destinationPath.c_str()) != 0) {
                    std::cout << "\033[1;32mFile moved successfully.\033[0m\n";
                } else {
                    std::cout << "\033[1;31mError moving file (code " << GetLastError() << ").\033[0m\n";
                }
            }
            else if (command == "cat" || command == "type") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: File path required.\033[0m\n";
                    continue;
                }
                std::wstring targetFile = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[1]);
                FileOperator::CatFile(targetFile);
            }
            else if (command == "stat") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Path required.\033[0m\n";
                    continue;
                }
                std::wstring targetPath = PathHelper::NormalizePath(current_path, wargs[1]);
                FileOperator::StatPath(targetPath);
            }
            else if (command == "find") {
                if (wargs.size() < 2) {
                    std::cout << "\033[1;31mError: Search pattern required.\033[0m\n";
                    continue;
                }
                FileOperator::RunFind(current_path, wargs[1]);
            }
            else if (command == "diff") {
                if (wargs.size() != 3) {
                    std::cout << "Usage: diff <file1> <file2>\n";
                    continue;
                }

                std::wstring leftPath = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[1]);
                std::wstring rightPath = RemoteProtocolHelper::ResolvePathForContext(current_path, wargs[2]);
                FileOperator::ShowFileDiff(leftPath, rightPath);
            }
            else if (command == "history") {
                if (wargs.size() > 1 && wargs[1] == L"clear") {
                    commandHistory.clear();
                    std::cout << "\033[1;32mHistory cleared.\033[0m\n";
                } else {
                    if (commandHistory.empty()) {
                        std::cout << "<History is empty>\n";
                    } else {
                        for (size_t i = 0; i < commandHistory.size(); ++i) {
                            std::cout << std::setw(4) << (i + 1) << "  " << commandHistory[i] << "\n";
                        }
                    }
                }
            }
            else if (command == "confirm") {
                if (wargs.size() == 1 || (wargs.size() > 1 && wargs[1] == L"status")) {
                    std::cout << "Destructive confirmations are " << (confirmDestructive ? "ON" : "OFF") << ".\n";
                } else if (wargs[1] == L"on") {
                    confirmDestructive = true;
                    std::cout << "\033[1;32mDestructive confirmations enabled.\033[0m\n";
                } else if (wargs[1] == L"off") {
                    confirmDestructive = false;
                    std::cout << "\033[1;33mDestructive confirmations disabled.\033[0m\n";
                } else {
                    std::cout << "Usage: confirm [on|off|status]\n";
                }
            }
            else if (command == "help") {
                std::cout << "\n";
                std::cout << "\033[1;36mAvailable Commands:\033[0m\n"
                          << "\n"
                          << "  drive             - List all available drives with storage info\n"
                          << "  shares            - List available network shares\n"
                          << "  ls / dir [-h] [path] [-t] - List files/folders (hidden files hidden by default, use -h/--hidden; use -t/--tree for tree layout)\n"
                          << "  tree [path]       - List directory structure recursively in tree format for a path\n"
                          << "  cd <directory>    - Move to a different directory or remote URL (ftp:// or ssh://)\n"
                          << "  pwd               - Print current local directory or remote URI\n"
                          << "  mkdir <name>      - Create a new folder, including parent directories when needed (supports spaces/quotes)\n"
                          << "  rmdir <name>      - Remove an empty folder (confirmation controlled by 'confirm')\n"
                          << "  trash [-f] <path...> - Move file/folder(s) to recycle bin (-f skips confirmation)\n"
                          << "  rm [-r] [-f] <path> - Remove a file or recursively remove a directory with -r (-f skips confirmation)\n"
                          << "  touch [-c] <file...> - Create/update file timestamps (-c skips creation)\n"
                          << "  copy <src> <dst>  - Copy a file to a new path (supports spaces/quotes)\n"
                          << "  move <src> <dst>  - Move/rename a file to a new path (supports spaces/quotes)\n"
                          << "  cat / type <file> - Display text file content from local or remote paths\n"
                          << "  stat <path>       - Display detailed properties and metadata of a path\n"
                          << "  find <pattern>    - Recursively search for matching file/folder names\n"
                          << "  diff <f1> <f2>    - Compare two local text files line by line\n"
                          << "  history [clear]   - Show recent command history or clear it\n"
                          << "  confirm [on|off|status] - Toggle confirmations for destructive actions\n"
                          << "  exit / quit       - Terminate the program\n\n"
                          << "\033[1;36mRemote Protocols:\033[0m\n"
                          << "  ftp://host/path     - Browse directories and read files via FTP\n"
                          << "  ssh://user@host/path - Browse directories and read files via SSH when ssh.exe or plink.exe is installed\n"
                          << "  Examples:\n"
                          << "    cd ftp://ftp.example.com/pub\n"
                          << "    ls ftp://ftp.example.com/pub\n"
                          << "    cat ssh://user@example.com/home/user/file.txt\n\n"
                          << "\033[1;36mOutput Redirection:\033[0m\n"
                          << "  You can redirect command output to a file using '>' (overwrite) or '>>' (append).\n"
                          << "  Examples:\n"
                          << "    ls -t > tree_layout.txt\n"
                          << "    drive >> space_history.txt\n"
                          << "    cat file.txt > copy.txt\n";
            }
            else if (!command.empty()) {
                std::cout << "\033[1;31mUnknown command: '" << command << "'. Type 'help' for options.\033[0m\n";
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            std::cout << LR"(fsctl(1)            CrossShell for UNIX Reference Manual                 fsctl(1)

    NAME
        fsctl - interactive filesystem controller, remote browser, and explorer

    SYNOPSIS
        fsctl [OPTIONS]

    DESCRIPTION
        fsctl is an interactive filesystem shell and storage manager providing
        local disk inspection, network share navigation, FTP/SSH remote
        browsing, and safe destructive action confirmations.

    OPTIONS
        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    INTERACTIVE COMMANDS
        drive
            List available drives with storage info.

        shares
            List available network shares.

        ls, dir [-h] [-t]
            List files and directories.

        tree [path]
            Display directory structure in tree format.

        cd <directory>
            Change current directory or remote URL.

        pwd
            Print current local directory or remote URI.

        mkdir <name>
            Create a new folder hierarchy.

        rmdir <name>
            Remove an empty folder.

        trash [-f] <path>
            Move file or folder to Recycle Bin.

        rm [-r] [-f] <path>
            Remove a file or recursively remove a directory.

        touch [-c] <path>
            Create or update file timestamp.

        cat, type <file>
            Display text file content.

        stat <path>
            Display metadata of a path.

        find <pattern>
            Recursively search for matching filenames.

        diff <f1> <f2>
            Compare two local files line by line.

        history [clear]
            Show command history.

        confirm [on|off]
            Toggle confirmation for destructive operations.

        exit, quit
            Terminate the interactive shell.

    EXAMPLES
        fsctl
            Launch the interactive filesystem controller shell.

    CrossShell for UNIX                                                    fsctl(1)
)";
            return 0;
        }
        if (arg == "--version" || arg == "-V") {
            std::cout << "fsctl 1.0.0\n";
            return 0;
        }
    }
    FsctlApplication app;
    return app.Run();
}
