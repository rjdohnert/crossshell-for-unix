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
 *
 * nfsctl.cpp - Enterprise NT-Native Unicode Windows NFS Control & Diagnostic Utility
 *
 * Target Platform: Windows 10/11 / Windows Server 2016/2019/2022
 * Compiler:        Microsoft Visual C++ (cl.exe)
 *
 * Compilation Command:
 *   cl.exe /EHsc /std:c++17 /O2 /W4 nfsctl.cpp mpr.lib ws2_32.lib advapi32.lib netapi32.lib /Fe:nfsctl.exe
 */

/*
Single-File Index
-----------------
1. Platform/Win32 includes, definitions, and library linkage
2. Global enums, models, and RAII scopes (Winsock, Registry, NetAPI, SCM)
3. Shared output formatting, JSON serialization, and logging engines
4. Windows Security & Registry transaction management engines
5. Windows WNet mount & Network probe engines
6. Tool domain controllers (Mounts, NfsStat, ExportFs, ShowMount, NfsConf, NfsIdMap, RpcDebug)
7. Help documentation system and CLI argument parser
8. Application lifecycle controller and Unicode main entry point
*/

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <winnetwk.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <lm.h>
#include <sddl.h>

#include <stdio.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cstdarg>
#include <cwctype>
#include <memory>

#pragma comment(lib, "mpr.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "netapi32.lib")

// ============================================================================
// 1. DATA MODELS, ENUMS & RAII SUBSYSTEMS
// ============================================================================

enum class OutputMode {
    Text,
    Json,
    Csv,
    Tsv
};

enum class NfsProbeMode {
    Both,
    V3,
    V4
};

struct GlobalOptions {
    OutputMode outputMode = OutputMode::Text;
    NfsProbeMode nfsProbeMode = NfsProbeMode::Both;
    bool dryRun = false;
    bool confirm = false;
    bool forceWrite = false;
    bool backupReg = false;
    std::wstring backupRegPath;
    bool win32Exit = false;
    bool quiet = false;
    bool verbose = false;
    bool trace = false;
    int timeoutMs = 1500;
    int opTimeoutMs = 0;
    int addressFamily = AF_INET;
};

namespace NfsRegistry {
    static const wchar_t* kClientDefaultSubKey = L"SOFTWARE\\Microsoft\\ClientForNFS\\CurrentVersion\\Default";
    static const wchar_t* kServerExportsSubKey = L"SOFTWARE\\Microsoft\\ServerForNFS\\CurrentVersion\\exports";
    static const wchar_t* kValueTimeout = L"Timeout";
    static const wchar_t* kValueRetries = L"Retries";
    static const wchar_t* kValueMountType = L"MountType";
    static const wchar_t* kValueDebugFlags = L"DebugFlags";
}

struct WinNfsMountInfo {
    std::wstring localDrive;
    std::wstring remotePath;
    std::wstring providerName;
};

struct NfsIdMapResult {
    DWORD code = ERROR_SUCCESS;
    std::wstring account;
    std::wstring domain;
    std::wstring sid;
};

class WinsockSubsystem {
public:
    WinsockSubsystem() : m_status(0), m_initialized(false) {
        WSADATA wsa;
        m_status = WSAStartup(MAKEWORD(2, 2), &wsa);
        m_initialized = (m_status == 0);
    }

    ~WinsockSubsystem() {
        if (m_initialized) {
            WSACleanup();
        }
    }

    bool Ok() const { return m_initialized; }
    int Status() const { return m_status; }

private:
    int m_status;
    bool m_initialized;
};

class ScopedRegistryKey {
public:
    explicit ScopedRegistryKey(HKEY hKey = NULL) : m_hKey(hKey) {}

    ~ScopedRegistryKey() {
        Close();
    }

    ScopedRegistryKey(const ScopedRegistryKey&) = delete;
    ScopedRegistryKey& operator=(const ScopedRegistryKey&) = delete;

    ScopedRegistryKey(ScopedRegistryKey&& other) noexcept : m_hKey(other.m_hKey) {
        other.m_hKey = NULL;
    }

    ScopedRegistryKey& operator=(ScopedRegistryKey&& other) noexcept {
        if (this != &other) {
            Close();
            m_hKey = other.m_hKey;
            other.m_hKey = NULL;
        }
        return *this;
    }

    HKEY Get() const { return m_hKey; }
    HKEY* Receive() { Close(); return &m_hKey; }
    bool IsValid() const { return m_hKey != NULL; }

    void Close() {
        if (m_hKey != NULL) {
            RegCloseKey(m_hKey);
            m_hKey = NULL;
        }
    }

private:
    HKEY m_hKey;
};

class ScopedServiceHandle {
public:
    explicit ScopedServiceHandle(SC_HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedServiceHandle() {
        Close();
    }

    ScopedServiceHandle(const ScopedServiceHandle&) = delete;
    ScopedServiceHandle& operator=(const ScopedServiceHandle&) = delete;

    ScopedServiceHandle(ScopedServiceHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedServiceHandle& operator=(ScopedServiceHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    SC_HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != NULL; }

    void Close() {
        if (m_handle != NULL) {
            CloseServiceHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    SC_HANDLE m_handle;
};

// ============================================================================
// 2. CANCELLATION & OUTPUT FORMATTING ENGINE
// ============================================================================

static volatile LONG g_cancelRequested = 0;

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            InterlockedExchange(&g_cancelRequested, 1);
            return TRUE;
        default:
            return FALSE;
    }
}

class NfsOutputFormatter {
public:
    static bool IsCancelled() {
        return InterlockedCompareExchange(&g_cancelRequested, 0, 0) != 0;
    }

    static bool IsDeadlineExceeded(ULONGLONG startTick, const GlobalOptions& options) {
        if (options.opTimeoutMs <= 0) return false;
        ULONGLONG elapsed = GetTickCount64() - startTick;
        return elapsed > static_cast<ULONGLONG>(options.opTimeoutMs);
    }

    static DWORD CheckOperationState(ULONGLONG startTick, const GlobalOptions& options) {
        if (IsCancelled()) return ERROR_CANCELLED;
        if (IsDeadlineExceeded(startTick, options)) return WAIT_TIMEOUT;
        return ERROR_SUCCESS;
    }

    static void LogInfo(const GlobalOptions& options, const wchar_t* fmt, ...) {
        if (options.quiet) return;
        va_list args;
        va_start(args, fmt);
        vwprintf(fmt, args);
        va_end(args);
    }

    static void LogVerbose(const GlobalOptions& options, const wchar_t* fmt, ...) {
        if (!options.verbose && !options.trace) return;
        va_list args;
        va_start(args, fmt);
        vwprintf(fmt, args);
        va_end(args);
    }

    static void LogTrace(const GlobalOptions& options, const wchar_t* fmt, ...) {
        if (!options.trace) return;
        va_list args;
        va_start(args, fmt);
        vwprintf(fmt, args);
        va_end(args);
    }

    static std::wstring ToLowerWide(std::wstring input) {
        std::transform(input.begin(), input.end(), input.begin(),
                       [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
        return input;
    }

    static bool ContainsIcase(const std::wstring& haystack, const std::wstring& needle) {
        if (needle.empty()) return true;
        return ToLowerWide(haystack).find(ToLowerWide(needle)) != std::wstring::npos;
    }

    static std::wstring JsonEscape(const std::wstring& input) {
        std::wstring out;
        out.reserve(input.size() + 8);
        for (wchar_t c : input) {
            switch (c) {
                case L'\\': out += L"\\\\"; break;
                case L'\"': out += L"\\\""; break;
                case L'\n': out += L"\\n"; break;
                case L'\r': out += L"\\r"; break;
                case L'\t': out += L"\\t"; break;
                default: out.push_back(c); break;
            }
        }
        return out;
    }

    static std::wstring RegistryTypeLabel(DWORD type) {
        switch (type) {
            case REG_SZ: return L"REG_SZ";
            case REG_EXPAND_SZ: return L"REG_EXPAND_SZ";
            case REG_MULTI_SZ: return L"REG_MULTI_SZ";
            case REG_DWORD: return L"REG_DWORD";
            case REG_QWORD: return L"REG_QWORD";
            case REG_BINARY: return L"REG_BINARY";
            case REG_NONE: return L"REG_NONE";
            default: return L"REG_UNKNOWN";
        }
    }

    static std::wstring RegistryStringFromData(const BYTE* data, DWORD size) {
        if (data == NULL || size == 0) return L"";
        const wchar_t* text = reinterpret_cast<const wchar_t*>(data);
        size_t charCount = size / sizeof(wchar_t);
        std::wstring value(text, text + charCount);
        while (!value.empty() && value.back() == L'\0') {
            value.pop_back();
        }
        return value;
    }

    static std::wstring RegistryBinaryToHex(const BYTE* data, DWORD size) {
        static const wchar_t* kHex = L"0123456789ABCDEF";
        std::wstring out;
        out.reserve(static_cast<size_t>(size) * 2 + 2);
        out += L"0x";
        for (DWORD i = 0; i < size; ++i) {
            BYTE b = data[i];
            out.push_back(kHex[(b >> 4) & 0xF]);
            out.push_back(kHex[b & 0xF]);
        }
        return out;
    }

    static void AppendRegistryValueJson(std::wstringstream& out, DWORD type, const BYTE* data, DWORD size) {
        switch (type) {
            case REG_DWORD: {
                DWORD value = 0;
                if (data != NULL && size >= sizeof(DWORD)) {
                    memcpy(&value, data, sizeof(DWORD));
                }
                out << value;
                return;
            }
            case REG_QWORD: {
                ULONGLONG value = 0;
                if (data != NULL && size >= sizeof(ULONGLONG)) {
                    memcpy(&value, data, sizeof(ULONGLONG));
                }
                out << value;
                return;
            }
            case REG_MULTI_SZ: {
                std::wstring multi = RegistryStringFromData(data, size);
                out << L"[";
                bool first = true;
                size_t pos = 0;
                while (pos < multi.size()) {
                    size_t next = multi.find(L'\0', pos);
                    if (next == std::wstring::npos) next = multi.size();
                    std::wstring part = multi.substr(pos, next - pos);
                    if (part.empty()) break;
                    if (!first) out << L",";
                    first = false;
                    out << L'"' << JsonEscape(part) << L'"';
                    pos = next + 1;
                }
                out << L"]";
                return;
            }
            case REG_SZ:
            case REG_EXPAND_SZ:
            default: {
                if (type == REG_SZ || type == REG_EXPAND_SZ) {
                    out << L'"' << JsonEscape(RegistryStringFromData(data, size)) << L'"';
                } else {
                    out << L'"' << JsonEscape(RegistryBinaryToHex(data, size)) << L'"';
                }
                return;
            }
        }
    }

    static DWORD BuildRegistryValuesJson(HKEY hKeyRoot, const wchar_t* subKey, std::wstring& outJson, DWORD& valueCount) {
        outJson.clear();
        valueCount = 0;

        ScopedRegistryKey hKey;
        DWORD dwErr = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, hKey.Receive());
        if (dwErr != ERROR_SUCCESS) {
            outJson = L"[]";
            return dwErr;
        }

        DWORD maxValueNameLen = 0;
        DWORD maxValueDataLen = 0;
        dwErr = RegQueryInfoKeyW(hKey.Get(), NULL, NULL, NULL, NULL, NULL, NULL, &valueCount,
                                 &maxValueNameLen, &maxValueDataLen, NULL, NULL);
        if (dwErr != ERROR_SUCCESS) {
            outJson = L"[]";
            return dwErr;
        }

        std::vector<wchar_t> nameBuffer(static_cast<size_t>(maxValueNameLen) + 2, L'\0');
        std::vector<BYTE> dataBuffer(std::max<DWORD>(maxValueDataLen, static_cast<DWORD>(sizeof(ULONGLONG))) + 2, 0);
        std::wstringstream payload;
        payload << L"[";
        bool first = true;

        for (DWORD index = 0; index < valueCount; ++index) {
            DWORD nameLen = static_cast<DWORD>(nameBuffer.size());
            DWORD type = 0;
            DWORD dataLen = static_cast<DWORD>(dataBuffer.size());
            dwErr = RegEnumValueW(hKey.Get(), index, nameBuffer.data(), &nameLen, NULL, &type, dataBuffer.data(), &dataLen);
            if (dwErr != ERROR_SUCCESS) {
                continue;
            }

            if (!first) payload << L",";
            first = false;
            payload << L"{\"name\":\"" << JsonEscape(std::wstring(nameBuffer.data(), nameLen))
                    << L"\",\"type\":\"" << RegistryTypeLabel(type) << L"\",\"value\":";
            AppendRegistryValueJson(payload, type, dataBuffer.data(), dataLen);
            payload << L"}";
        }

        payload << L"]";
        outJson = payload.str();
        return ERROR_SUCCESS;
    }

    static std::wstring Win32ErrorMessage(DWORD code) {
        LPWSTR buffer = nullptr;
        DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
        DWORD len = FormatMessageW(flags, NULL, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, NULL);
        if (len == 0 || buffer == nullptr) {
            return L"Unknown error";
        }
        std::wstring msg(buffer, len);
        LocalFree(buffer);
        while (!msg.empty() && (msg.back() == L'\r' || msg.back() == L'\n')) {
            msg.pop_back();
        }
        return msg;
    }

    static void PrintWin32Error(const wchar_t* context, DWORD code) {
        fwprintf(stderr, L"Error: %s: %s (Win32 Error %lu)\n", context, Win32ErrorMessage(code).c_str(), code);
    }

    static int NormalizeExitCode(DWORD code) {
        if (code == ERROR_SUCCESS) return 0;
        if (code == ERROR_INVALID_PARAMETER || code == ERROR_BAD_ARGUMENTS) return 1;
        if (code == ERROR_ACCESS_DENIED || code == ERROR_ELEVATION_REQUIRED || code == ERROR_CANCELLED) return 2;
        if (code == ERROR_NETWORK_UNREACHABLE || code == ERROR_HOST_UNREACHABLE || code == ERROR_CONNECTION_REFUSED || code == ERROR_DEV_NOT_EXIST) return 3;
        return 4;
    }

    static std::wstring AddressFamilyLabel(int addressFamily) {
        if (addressFamily == AF_INET6) return L"ipv6";
        if (addressFamily == AF_UNSPEC) return L"dual";
        return L"ipv4";
    }

    static std::wstring NfsProbeModeLabel(NfsProbeMode mode) {
        switch (mode) {
            case NfsProbeMode::V3: return L"v3";
            case NfsProbeMode::V4: return L"v4";
            case NfsProbeMode::Both:
            default: return L"both";
        }
    }

    static void PrintDelimitedCell(const std::wstring& value, OutputMode mode) {
        if (mode == OutputMode::Csv) {
            std::wstring escaped;
            escaped.reserve(value.size() + 4);
            for (wchar_t c : value) {
                if (c == L'"') escaped += L"\"\"";
                else escaped.push_back(c);
            }
            wprintf(L"\"%s\"", escaped.c_str());
        } else {
            std::wstring clean = value;
            std::replace(clean.begin(), clean.end(), L'\t', L' ');
            std::replace(clean.begin(), clean.end(), L'\n', L' ');
            wprintf(L"%s", clean.c_str());
        }
    }

    static void PrintDelimitedRow(const std::vector<std::wstring>& cells, OutputMode mode) {
        const wchar_t* sep = (mode == OutputMode::Csv) ? L"," : L"\t";
        for (size_t i = 0; i < cells.size(); ++i) {
            if (i > 0) wprintf(L"%s", sep);
            PrintDelimitedCell(cells[i], mode);
        }
        wprintf(L"\n");
    }

    static void PrintJsonEnvelope(const std::wstring& command, DWORD code, const std::wstring& dataJson, const std::wstring& errorMessage = L"") {
        std::wstring err = errorMessage.empty() ? Win32ErrorMessage(code) : errorMessage;
        std::wstring errJson = L"null";
        if (code != ERROR_SUCCESS) {
            errJson = L"\"" + JsonEscape(err) + L"\"";
        }
        wprintf(L"{\"ok\":%s,\"code\":%lu,\"command\":\"%s\",\"data\":%s,\"error\":%s}\n",
                (code == ERROR_SUCCESS) ? L"true" : L"false",
                code,
                JsonEscape(command).c_str(),
                dataJson.c_str(),
                errJson.c_str());
    }
};

// ============================================================================
// 3. SECURITY & REGISTRY TRANSACTION ENGINE
// ============================================================================

class NfsSecurityManager {
public:
    static BOOL IsAdminElevated() {
        BOOL isAdmin = FALSE;
        PSID adminGroup = NULL;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(NULL, adminGroup, &isAdmin);
            FreeSid(adminGroup);
        }
        return isAdmin;
    }

    static DWORD RequireAdmin(const wchar_t* operation) {
        if (!IsAdminElevated()) {
            fwprintf(stderr, L"Error: Operation '%s' requires elevated Administrator privileges.\n", operation);
            return ERROR_ACCESS_DENIED;
        }
        return ERROR_SUCCESS;
    }

    static BOOL SafeWstol(const wchar_t* str, LONG* outVal) {
        if (!str || *str == L'\0') return FALSE;
        wchar_t* endPtr = NULL;
        LONG val = wcstol(str, &endPtr, 10);
        if (endPtr == str || *endPtr != L'\0') return FALSE;
        *outVal = val;
        return TRUE;
    }

    static NfsIdMapResult ResolveAccountToSid(const wchar_t* account_name) {
        NfsIdMapResult result;
        result.account = account_name ? account_name : L"";

        if (!account_name || !*account_name) {
            result.code = ERROR_INVALID_PARAMETER;
            return result;
        }

        DWORD sidSize = 0;
        DWORD domainSize = 0;
        SID_NAME_USE sidUse = SidTypeUnknown;
        LookupAccountNameW(nullptr, account_name, nullptr, &sidSize, nullptr, &domainSize, &sidUse);

        DWORD lookupErr = GetLastError();
        if (lookupErr != ERROR_INSUFFICIENT_BUFFER) {
            result.code = lookupErr;
            return result;
        }

        std::vector<BYTE> sidBuffer(sidSize);
        std::vector<wchar_t> domainBuffer(domainSize > 0 ? domainSize : 1, L'\0');

        if (!LookupAccountNameW(nullptr, account_name,
                                sidBuffer.data(), &sidSize,
                                domainBuffer.data(), &domainSize,
                                &sidUse)) {
            result.code = GetLastError();
            return result;
        }

        LPWSTR sidString = nullptr;
        if (!ConvertSidToStringSidW(reinterpret_cast<PSID>(sidBuffer.data()), &sidString)) {
            result.code = GetLastError();
            return result;
        }

        result.domain.assign(domainBuffer.data());
        result.sid.assign(sidString ? sidString : L"");
        LocalFree(sidString);
        result.code = ERROR_SUCCESS;
        return result;
    }
};

class NfsRegistryEngine {
public:
    static DWORD WinRegGetDWORD(HKEY hKeyRoot, const wchar_t* subKey, const wchar_t* valueName, DWORD* outValue) {
        ScopedRegistryKey hKey;
        DWORD dwErr = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, hKey.Receive());
        if (dwErr != ERROR_SUCCESS) return dwErr;

        DWORD type = 0;
        DWORD size = sizeof(DWORD);
        dwErr = RegQueryValueExW(hKey.Get(), valueName, NULL, &type, reinterpret_cast<LPBYTE>(outValue), &size);
        return (type == REG_DWORD) ? dwErr : ERROR_INVALID_DATATYPE;
    }

    static DWORD WinRegSetDWORD(HKEY hKeyRoot, const wchar_t* subKey, const wchar_t* valueName, DWORD value) {
        ScopedRegistryKey hKey;
        DWORD dwErr = RegCreateKeyExW(hKeyRoot, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, hKey.Receive(), NULL);
        if (dwErr != ERROR_SUCCESS) return dwErr;

        dwErr = RegSetValueExW(hKey.Get(), valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
        return dwErr;
    }

    static DWORD WinRegEnumSubkeys(HKEY hKeyRoot, const wchar_t* subKey, std::vector<std::wstring>& subkeys) {
        ScopedRegistryKey hKey;
        DWORD dwErr = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, hKey.Receive());
        if (dwErr != ERROR_SUCCESS) return dwErr;

        DWORD index = 0;
        wchar_t name[256];
        DWORD nameLen = sizeof(name) / sizeof(wchar_t);
        while (RegEnumKeyExW(hKey.Get(), index++, name, &nameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
            subkeys.push_back(name);
            nameLen = sizeof(name) / sizeof(wchar_t);
        }
        return ERROR_SUCCESS;
    }

    static DWORD BackupRegistryValue(const wchar_t* subKey, const wchar_t* valueName, DWORD existingVal, DWORD readErr,
                                     const GlobalOptions& options, std::wstring& outPath) {
        wchar_t defaultTemp[MAX_PATH] = {0};
        if (!options.backupRegPath.empty()) {
            outPath = options.backupRegPath;
        } else {
            DWORD len = GetTempPathW(MAX_PATH, defaultTemp);
            if (len == 0 || len >= MAX_PATH) {
                return ERROR_PATH_NOT_FOUND;
            }
            SYSTEMTIME st{};
            GetLocalTime(&st);
            wchar_t fileName[128] = {0};
            swprintf(fileName, 128, L"nfsctl-reg-backup-%04u%02u%02u-%02u%02u%02u.txt",
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            outPath = std::wstring(defaultTemp) + fileName;
        }

        std::wofstream out(outPath, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            return ERROR_OPEN_FAILED;
        }

        out << L"Hive=HKEY_LOCAL_MACHINE\n";
        out << L"SubKey=" << subKey << L"\n";
        out << L"ValueName=" << valueName << L"\n";
        if (readErr == ERROR_SUCCESS) {
            out << L"ValueType=REG_DWORD\n";
            out << L"Value=" << existingVal << L"\n";
        } else {
            out << L"ValueType=MISSING\n";
            out << L"ReadError=" << readErr << L"\n";
        }
        out.close();
        return ERROR_SUCCESS;
    }

    static DWORD BackupRegistryValueIfRequested(const wchar_t* subKey, const wchar_t* valueName, DWORD existingVal,
                                                DWORD readErr, const GlobalOptions& options) {
        if (!options.backupReg) {
            return ERROR_SUCCESS;
        }

        std::wstring backupPath;
        DWORD backupErr = BackupRegistryValue(subKey, valueName, existingVal, readErr, options, backupPath);
        if (backupErr != ERROR_SUCCESS) {
            NfsOutputFormatter::PrintWin32Error(L"failed to create registry backup", backupErr);
            return backupErr;
        }

        NfsOutputFormatter::LogVerbose(options, L"[verbose] Registry backup saved: %s\n", backupPath.c_str());
        return ERROR_SUCCESS;
    }

    static DWORD PrepareWriteOperation(const GlobalOptions& options, ULONGLONG startTick, const wchar_t* operation,
                                       const std::wstring& dryRunMessage, bool& shouldExecute, bool requireAdmin = true) {
        shouldExecute = false;
        DWORD stateErr = NfsOutputFormatter::CheckOperationState(startTick, options);
        if (stateErr != ERROR_SUCCESS) return stateErr;

        if (options.dryRun) {
            NfsOutputFormatter::LogInfo(options, L"%s", dryRunMessage.c_str());
            return ERROR_SUCCESS;
        }

        if (!options.confirm) {
            fwprintf(stderr, L"Error: %s requires --confirm (or use --dry-run).\n", operation);
            return ERROR_CANCELLED;
        }

        if (requireAdmin) {
            DWORD adminErr = NfsSecurityManager::RequireAdmin(operation);
            if (adminErr != ERROR_SUCCESS) return adminErr;
        }

        shouldExecute = true;
        return ERROR_SUCCESS;
    }
};

// ============================================================================
// 4. MOUNT & PROBE ENGINES
// ============================================================================

class NfsMountManager {
public:
    static bool IsLikelyNfsMount(const WinNfsMountInfo& mount) {
        if (mount.remotePath.empty()) return false;
        if (NfsOutputFormatter::ContainsIcase(mount.providerName, L"nfs")) return true;
        if (NfsOutputFormatter::ContainsIcase(mount.providerName, L"client for nfs")) return true;
        if (NfsOutputFormatter::ContainsIcase(mount.providerName, L"microsoft windows network")) return false;
        return false;
    }

    static bool IsNfsClientAvailable() {
        ScopedRegistryKey hKey;
        bool hasRegistry = (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                          NfsRegistry::kClientDefaultSubKey,
                                          0, KEY_READ, hKey.Receive()) == ERROR_SUCCESS);

        ScopedServiceHandle scm(OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT));
        bool hasService = false;
        if (scm.IsValid()) {
            ScopedServiceHandle svc(OpenServiceW(scm.Get(), L"NfsClnt", SERVICE_QUERY_STATUS));
            if (svc.IsValid()) {
                hasService = true;
            }
        }
        return hasRegistry || hasService;
    }

    static DWORD EnumWindowsNetworkMounts(std::vector<WinNfsMountInfo>& mounts, const GlobalOptions& options, ULONGLONG startTick) {
        if (!IsNfsClientAvailable()) {
            return ERROR_NOT_SUPPORTED;
        }

        HANDLE hEnum = NULL;
        DWORD dwErr = WNetOpenEnumW(RESOURCE_CONNECTED, RESOURCETYPE_DISK, 0, NULL, &hEnum);
        if (dwErr != NO_ERROR || hEnum == NULL) return dwErr;

        DWORD count = 0xFFFFFFFF;
        DWORD bufferSize = 16384;
        std::vector<BYTE> buffer(bufferSize);

        while (true) {
            DWORD stateErr = NfsOutputFormatter::CheckOperationState(startTick, options);
            if (stateErr != ERROR_SUCCESS) {
                WNetCloseEnum(hEnum);
                return stateErr;
            }

            count = 0xFFFFFFFF;
            bufferSize = static_cast<DWORD>(buffer.size());
            dwErr = WNetEnumResourceW(hEnum, &count, buffer.data(), &bufferSize);

            if (dwErr == ERROR_MORE_DATA) {
                buffer.resize(bufferSize);
                continue;
            }
            if (dwErr != NO_ERROR || count == 0) break;

            LPNETRESOURCEW pRsrc = reinterpret_cast<LPNETRESOURCEW>(buffer.data());
            for (DWORD i = 0; i < count; ++i) {
                DWORD rowStateErr = NfsOutputFormatter::CheckOperationState(startTick, options);
                if (rowStateErr != ERROR_SUCCESS) {
                    WNetCloseEnum(hEnum);
                    return rowStateErr;
                }

                WinNfsMountInfo info;
                info.localDrive = pRsrc[i].lpLocalName ? pRsrc[i].lpLocalName : L"";
                info.remotePath = pRsrc[i].lpRemoteName ? pRsrc[i].lpRemoteName : L"";
                info.providerName = pRsrc[i].lpProvider ? pRsrc[i].lpProvider : L"";
                if (IsLikelyNfsMount(info)) {
                    mounts.push_back(info);
                }
            }
        }
        WNetCloseEnum(hEnum);
        return ERROR_SUCCESS;
    }

    static DWORD WinMountNfsShare(const wchar_t* remotePath, const wchar_t* localDrive) {
        NETRESOURCEW nr = {0};
        nr.dwType = RESOURCETYPE_DISK;
        nr.lpLocalName = const_cast<wchar_t*>(localDrive);
        nr.lpRemoteName = const_cast<wchar_t*>(remotePath);

        DWORD dwErr = WNetAddConnection2W(&nr, NULL, NULL, CONNECT_TEMPORARY);
        if (dwErr != NO_ERROR) {
            NfsOutputFormatter::PrintWin32Error(L"WNetAddConnection2W failed", dwErr);
        }
        return dwErr;
    }

    static DWORD WinUnmountNfsShare(const wchar_t* localDrive) {
        DWORD dwErr = WNetCancelConnection2W(localDrive, CONNECT_UPDATE_PROFILE, TRUE);
        if (dwErr != NO_ERROR) {
            NfsOutputFormatter::PrintWin32Error(L"WNetCancelConnection2W failed", dwErr);
        }
        return dwErr;
    }
};

class NfsProbeEngine {
public:
    static BOOL ProbeTcpPort(const wchar_t* host, int port, int timeout_ms, int address_family) {
        struct addrinfoW hints = {0}, *res = nullptr;
        hints.ai_family = address_family;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        wchar_t portStr[16];
        swprintf(portStr, 16, L"%d", port);

        if (GetAddrInfoW(host, portStr, &hints, &res) != 0 || !res) {
            return FALSE;
        }

        BOOL connected = FALSE;
        for (addrinfoW* it = res; it != nullptr && !connected; it = it->ai_next) {
            SOCKET s = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
            if (s == INVALID_SOCKET) {
                continue;
            }

            u_long mode = 1;
            ioctlsocket(s, FIONBIO, &mode);
            connect(s, it->ai_addr, static_cast<int>(it->ai_addrlen));

            fd_set wset, eset;
            FD_ZERO(&wset);
            FD_ZERO(&eset);
            FD_SET(s, &wset);
            FD_SET(s, &eset);

            struct timeval tv;
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            if (select(0, NULL, &wset, &eset, &tv) > 0 && FD_ISSET(s, &wset) && !FD_ISSET(s, &eset)) {
                connected = TRUE;
            }
            closesocket(s);
        }

        FreeAddrInfoW(res);
        return connected;
    }
};

// ============================================================================
// 5. TOOL DOMAIN CONTROLLERS
// ============================================================================

class MountsToolController {
public:
    static void PrintHelp() {
        wprintf(L"USAGE: nfsctl.exe --mounts [--json]\n"
                L"       nfsctl.exe --mount <remote-path> <drive:> [--confirm|--dry-run]\n"
                L"       nfsctl.exe --umount <drive:> [--confirm|--dry-run]\n");
    }

    static DWORD Execute(const std::vector<std::wstring>& args, const GlobalOptions& options) {
        ULONGLONG startTick = GetTickCount64();

        std::wstring mount_path = L"", mount_drive = L"", umount_drive = L"";

        auto IsSwitchToken = [](const std::wstring& token) {
            return !token.empty() && token[0] == L'-';
        };

        const std::wstring command = args.empty() ? L"" : args[0];
        if (command == L"--mount") {
            if (args.size() != 3 || IsSwitchToken(args[1]) || IsSwitchToken(args[2])) {
                fwprintf(stderr, L"Error: Usage: nfsctl.exe --mount <remote-path> <drive:>\n");
                return ERROR_INVALID_PARAMETER;
            }
            mount_path = args[1];
            mount_drive = args[2];
        } else if (command == L"--umount") {
            if (args.size() != 2 || IsSwitchToken(args[1])) {
                fwprintf(stderr, L"Error: Usage: nfsctl.exe --umount <drive:>\n");
                return ERROR_INVALID_PARAMETER;
            }
            umount_drive = args[1];
        } else if (command == L"--mounts" || command == L"-m") {
            if (args.size() != 1) {
                fwprintf(stderr, L"Error: --mounts does not accept additional arguments.\n");
                return ERROR_INVALID_PARAMETER;
            }
        }

        if (!mount_path.empty() && !mount_drive.empty()) {
            bool shouldExecute = false;
            std::wstring dryRunMessage = L"[dry-run] Would mount " + mount_path + L" to " + mount_drive + L"\n";
            DWORD prepErr = NfsRegistryEngine::PrepareWriteOperation(options, startTick, L"mount operation", dryRunMessage, shouldExecute, false);
            if (prepErr != ERROR_SUCCESS || !shouldExecute) return prepErr;

            NfsOutputFormatter::LogInfo(options, L"[*] Mounting %s to %s...\n", mount_path.c_str(), mount_drive.c_str());
            return NfsMountManager::WinMountNfsShare(mount_path.c_str(), mount_drive.c_str());
        }

        if (!umount_drive.empty()) {
            bool shouldExecute = false;
            std::wstring dryRunMessage = L"[dry-run] Would unmount " + umount_drive + L"\n";
            DWORD prepErr = NfsRegistryEngine::PrepareWriteOperation(options, startTick, L"unmount operation", dryRunMessage, shouldExecute, false);
            if (prepErr != ERROR_SUCCESS || !shouldExecute) return prepErr;

            NfsOutputFormatter::LogInfo(options, L"[*] Unmounting %s...\n", umount_drive.c_str());
            return NfsMountManager::WinUnmountNfsShare(umount_drive.c_str());
        }

        if (options.outputMode == OutputMode::Text) {
            NfsOutputFormatter::LogInfo(options, L"[ nfsctl : Connected Windows Network Drives ]\n\n");
        }

        std::vector<WinNfsMountInfo> mounts;
        DWORD dwErr = NfsMountManager::EnumWindowsNetworkMounts(mounts, options, startTick);
        if (dwErr != ERROR_SUCCESS) {
            if (options.outputMode == OutputMode::Json) {
                NfsOutputFormatter::PrintJsonEnvelope(L"mounts", dwErr, L"{\"mounts\":[],\"count\":0}");
            } else {
                NfsOutputFormatter::PrintWin32Error(L"failed to enumerate network mounts", dwErr);
            }
            return dwErr;
        }

        if (options.outputMode == OutputMode::Json) {
            std::wstringstream payload;
            payload << L"{\"mounts\":[";
            for (size_t i = 0; i < mounts.size(); ++i) {
                if (i > 0) payload << L",";
                payload << L"{\"localDrive\":\"" << NfsOutputFormatter::JsonEscape(mounts[i].localDrive)
                        << L"\",\"remotePath\":\"" << NfsOutputFormatter::JsonEscape(mounts[i].remotePath)
                        << L"\",\"provider\":\"" << NfsOutputFormatter::JsonEscape(mounts[i].providerName) << L"\"}";
            }
            payload << L"],\"count\":" << mounts.size() << L"}";
            NfsOutputFormatter::PrintJsonEnvelope(L"mounts", ERROR_SUCCESS, payload.str());
            return ERROR_SUCCESS;
        }

        if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
            NfsOutputFormatter::PrintDelimitedRow({L"localDrive", L"remotePath", L"provider"}, options.outputMode);
            for (const auto& m : mounts) {
                NfsOutputFormatter::PrintDelimitedRow({m.localDrive, m.remotePath, m.providerName}, options.outputMode);
            }
            return ERROR_SUCCESS;
        }

        if (mounts.empty()) {
            NfsOutputFormatter::LogInfo(options, L"No active NFS mounts detected via WNet API.\n");
            return ERROR_SUCCESS;
        }

        wprintf(L"%-12s %-35s %s\n", L"Local Drive", L"Remote Path", L"Provider");
        wprintf(L"----------------------------------------------------------------------\n");

        for (const auto& m : mounts) {
            wprintf(L"%-12s %-35s %s\n", m.localDrive.empty() ? L"(N/A)" : m.localDrive.c_str(),
                    m.remotePath.c_str(), m.providerName.c_str());
        }
        return ERROR_SUCCESS;
    }
};

class NfsStatToolController {
public:
    static void PrintHelp() {
        wprintf(L"USAGE: nfsctl.exe --nfsstat [--json]\n"
                L"Displays ClientForNFS registry configuration values (Timeout, Retries, MountType).\n");
    }

    static DWORD Execute(const GlobalOptions& options) {
        ULONGLONG startTick = GetTickCount64();
        DWORD stateErr = NfsOutputFormatter::CheckOperationState(startTick, options);
        if (stateErr != ERROR_SUCCESS) return stateErr;

        if (options.outputMode == OutputMode::Text) {
            NfsOutputFormatter::LogInfo(options, L"[ nfsctl : Windows Client for NFS Status ]\n\n");
        }

        const wchar_t* subKey = NfsRegistry::kClientDefaultSubKey;
        DWORD timeout = 0, retries = 0, mountType = 0;

        DWORD r1 = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueTimeout, &timeout);
        DWORD r2 = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueRetries, &retries);
        DWORD r3 = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueMountType, &mountType);

        if (options.outputMode == OutputMode::Json) {
            std::wstringstream payload;
            payload << L"{\"subKey\":\"" << NfsOutputFormatter::JsonEscape(subKey) << L"\",\"timeout\":";
            payload << ((r1 == ERROR_SUCCESS) ? std::to_wstring(timeout) : L"null");
            payload << L",\"retries\":" << ((r2 == ERROR_SUCCESS) ? std::to_wstring(retries) : L"null");
            payload << L",\"mountType\":" << ((r3 == ERROR_SUCCESS) ? std::to_wstring(mountType) : L"null");
            payload << L",\"mountTypeLabel\":";
            if (r3 == ERROR_SUCCESS) {
                payload << L"\"" << (mountType == 1 ? L"Hard" : L"Soft") << L"\"";
            } else {
                payload << L"null";
            }
            payload << L"}";
            DWORD status = (r1 == ERROR_SUCCESS || r2 == ERROR_SUCCESS || r3 == ERROR_SUCCESS) ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
            NfsOutputFormatter::PrintJsonEnvelope(L"nfsstat", status, payload.str());
            return status;
        }

        if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
            NfsOutputFormatter::PrintDelimitedRow({L"key", L"value"}, options.outputMode);
            NfsOutputFormatter::PrintDelimitedRow({L"Timeout", (r1 == ERROR_SUCCESS) ? std::to_wstring(timeout) : L"N/A"}, options.outputMode);
            NfsOutputFormatter::PrintDelimitedRow({L"Retries", (r2 == ERROR_SUCCESS) ? std::to_wstring(retries) : L"N/A"}, options.outputMode);
            NfsOutputFormatter::PrintDelimitedRow({L"MountType", (r3 == ERROR_SUCCESS) ? std::to_wstring(mountType) : L"N/A"}, options.outputMode);
            NfsOutputFormatter::PrintDelimitedRow({L"MountTypeLabel", (r3 == ERROR_SUCCESS) ? (mountType == 1 ? L"Hard" : L"Soft") : L"N/A"}, options.outputMode);
            return (r1 == ERROR_SUCCESS || r2 == ERROR_SUCCESS || r3 == ERROR_SUCCESS) ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND;
        }

        if (r1 == ERROR_SUCCESS || r2 == ERROR_SUCCESS || r3 == ERROR_SUCCESS) {
            wprintf(L"ClientForNFS Registry Configuration (%s):\n", subKey);
            wprintf(L"  Timeout (sec) : %s\n", (r1 == ERROR_SUCCESS) ? std::to_wstring(timeout).c_str() : L"N/A");
            wprintf(L"  Retries       : %s\n", (r2 == ERROR_SUCCESS) ? std::to_wstring(retries).c_str() : L"N/A");
            wprintf(L"  Mount Type    : %s\n", (r3 == ERROR_SUCCESS) ? (mountType == 1 ? L"Hard" : L"Soft") : L"N/A");
            return ERROR_SUCCESS;
        }

        NfsOutputFormatter::LogInfo(options, L"[!] Client for NFS Registry keys not found under HKLM\\%s.\n", subKey);
        return ERROR_FILE_NOT_FOUND;
    }
};

class ExportFsToolController {
public:
    static void PrintHelp() {
        wprintf(L"USAGE: nfsctl.exe --exportfs\n"
                L"Lists local ServerForNFS export keys and falls back to NetApi32 share enumeration when absent.\n");
    }

    static DWORD ToolExportFs(bool suppressOutput = false, std::wstring* jsonPayload = nullptr) {
        struct ShareRecord {
            std::wstring name;
            std::wstring path;
        };

        if (!suppressOutput) {
            wprintf(L"[ nfsctl : Local Windows Server for NFS Exports ]\n\n");
        }

        const wchar_t* exportKey = NfsRegistry::kServerExportsSubKey;
        std::vector<std::wstring> subkeys;
        std::vector<ShareRecord> shares;
        NfsRegistryEngine::WinRegEnumSubkeys(HKEY_LOCAL_MACHINE, exportKey, subkeys);

        auto buildRegistryExportJson = [&](const std::vector<std::wstring>& keys, const std::vector<ShareRecord>& localShares,
                                           const std::wstring& source) -> std::wstring {
            std::wstringstream payload;
            payload << L"{\"exports\":[";
            for (size_t i = 0; i < keys.size(); ++i) {
                if (i > 0) payload << L",";
                std::wstring valuesJson;
                DWORD valueCount = 0;
                std::wstring fullKey = std::wstring(exportKey) + L"\\" + keys[i];
                DWORD valuesErr = NfsOutputFormatter::BuildRegistryValuesJson(HKEY_LOCAL_MACHINE, fullKey.c_str(), valuesJson, valueCount);
                payload << L"{\"id\":\"" << NfsOutputFormatter::JsonEscape(keys[i]) << L"\",\"valueCount\":" << valueCount
                        << L",\"values\":" << (valuesJson.empty() ? L"[]" : valuesJson);
                if (valuesErr != ERROR_SUCCESS) {
                    payload << L",\"valueError\":" << valuesErr;
                }
                payload << L"}";
            }
            payload << L"],\"shares\":[";
            for (size_t i = 0; i < localShares.size(); ++i) {
                if (i > 0) payload << L",";
                payload << L"{\"name\":\"" << NfsOutputFormatter::JsonEscape(localShares[i].name)
                        << L"\",\"path\":\"" << NfsOutputFormatter::JsonEscape(localShares[i].path) << L"\"}";
            }
            payload << L"],\"source\":\"" << NfsOutputFormatter::JsonEscape(source) << L"\",\"count\":" << keys.size()
                    << L",\"shareCount\":" << localShares.size() << L"}";
            return payload.str();
        };

        if (subkeys.empty()) {
            if (!suppressOutput) {
                wprintf(L"No local exports found under HKLM\\%s\n", exportKey);
                wprintf(L"NetApi32 Share Fallback: showing share entries instead.\n\n");
            }

            PSHARE_INFO_502 BufPtr = NULL;
            DWORD er = 0, tr = 0, resume = 0;
            NET_API_STATUS res = NetShareEnum(NULL, 502, reinterpret_cast<LPBYTE*>(&BufPtr), MAX_PREFERRED_LENGTH, &er, &tr, &resume);

            if (res == NERR_Success && BufPtr != NULL) {
                for (DWORD i = 0; i < er; ++i) {
                    shares.push_back({BufPtr[i].shi502_netname != NULL ? BufPtr[i].shi502_netname : L"",
                                      BufPtr[i].shi502_path != NULL ? BufPtr[i].shi502_path : L""});
                }
                if (!suppressOutput) {
                    wprintf(L"%-20s %s\n", L"NetApi Share Name", L"Local Path");
                    wprintf(L"--------------------------------------------------\n");
                    for (DWORD i = 0; i < er; ++i) {
                        wprintf(L"%-20s %s\n", BufPtr[i].shi502_netname, BufPtr[i].shi502_path);
                    }
                }
                NetApiBufferFree(BufPtr);
                if (jsonPayload != nullptr) {
                    *jsonPayload = buildRegistryExportJson(subkeys, shares, L"netapi");
                }
                return ERROR_SUCCESS;
            }
            if (jsonPayload != nullptr) {
                *jsonPayload = L"{\"exports\":[],\"shares\":[],\"source\":\"netapi\",\"count\":0,\"shareCount\":0}";
            }
            return res;
        }

        for (const auto& key : subkeys) {
            if (!suppressOutput) {
                std::wstring valuesJson;
                DWORD valueCount = 0;
                std::wstring fullKey = std::wstring(exportKey) + L"\\" + key;
                DWORD valuesErr = NfsOutputFormatter::BuildRegistryValuesJson(HKEY_LOCAL_MACHINE, fullKey.c_str(), valuesJson, valueCount);
                if (valuesErr == ERROR_SUCCESS) {
                    wprintf(L"  Export Entry ID: %s (%lu registry value%s)\n",
                            key.c_str(), valueCount, (valueCount == 1) ? L"" : L"s");
                } else {
                    wprintf(L"  Export Entry ID: %s (registry values unavailable)\n", key.c_str());
                }
            }
        }

        if (jsonPayload != nullptr) {
            *jsonPayload = buildRegistryExportJson(subkeys, shares, L"registry");
        }

        return ERROR_SUCCESS;
    }

    static DWORD Execute(const GlobalOptions& options) {
        if (options.outputMode == OutputMode::Json) {
            std::wstring payload;
            DWORD rc = ToolExportFs(true, &payload);
            NfsOutputFormatter::PrintJsonEnvelope(L"exportfs", rc, payload.empty() ? L"{\"exports\":[],\"shares\":[]}" : payload);
            return rc;
        }
        return ToolExportFs(false);
    }
};

class ShowMountToolController {
public:
    static void PrintHelp() {
        wprintf(L"USAGE: nfsctl.exe --showmount <host> [--nfsv3|--nfsv4|--nfsboth] [--timeout-ms <n>] [--ipv4|--ipv6|--dual-stack] [--json]\n"
                L"Probes NFSv3 via Portmapper/111 plus 2049, and NFSv4 via 2049 directly.\n");
    }

    static DWORD Execute(const wchar_t* remote_host, const GlobalOptions& options) {
        if (options.outputMode == OutputMode::Text) {
            NfsOutputFormatter::LogInfo(options, L"[ nfsctl : Probing Remote NFS Server (%s) ]\n\n", remote_host);
        }
        NfsOutputFormatter::LogTrace(options, L"[trace] probing host=%s family=%s timeoutMs=%d nfsMode=%s\n", remote_host,
                 NfsOutputFormatter::AddressFamilyLabel(options.addressFamily).c_str(), options.timeoutMs, NfsOutputFormatter::NfsProbeModeLabel(options.nfsProbeMode).c_str());

        BOOL nfs3_portmapper = FALSE;
        BOOL nfs3_port2049 = FALSE;
        BOOL nfs4_port2049 = FALSE;

        if (options.nfsProbeMode == NfsProbeMode::Both || options.nfsProbeMode == NfsProbeMode::V3) {
            nfs3_portmapper = NfsProbeEngine::ProbeTcpPort(remote_host, 111, options.timeoutMs, options.addressFamily);
            nfs3_port2049   = NfsProbeEngine::ProbeTcpPort(remote_host, 2049, options.timeoutMs, options.addressFamily);
        }
        if (options.nfsProbeMode == NfsProbeMode::Both || options.nfsProbeMode == NfsProbeMode::V4) {
            nfs4_port2049 = NfsProbeEngine::ProbeTcpPort(remote_host, 2049, options.timeoutMs, options.addressFamily);
        }

        BOOL nfs3_online = (nfs3_portmapper || nfs3_port2049);
        BOOL nfs4_online = nfs4_port2049;
        BOOL showV3 = (options.nfsProbeMode == NfsProbeMode::Both || options.nfsProbeMode == NfsProbeMode::V3);
        BOOL showV4 = (options.nfsProbeMode == NfsProbeMode::Both || options.nfsProbeMode == NfsProbeMode::V4);

        DWORD status = ((showV3 && nfs3_online) || (showV4 && nfs4_online)) ? ERROR_SUCCESS : ERROR_DEV_NOT_EXIST;

        if (options.outputMode == OutputMode::Json) {
            std::wstringstream payload;
            payload << L"{\"host\":\"" << NfsOutputFormatter::JsonEscape(remote_host)
                    << L"\",\"family\":\"" << NfsOutputFormatter::JsonEscape(NfsOutputFormatter::AddressFamilyLabel(options.addressFamily))
                    << L"\",\"nfsMode\":\"" << NfsOutputFormatter::JsonEscape(NfsOutputFormatter::NfsProbeModeLabel(options.nfsProbeMode))
                    << L"\",\"timeoutMs\":" << options.timeoutMs
                    << L",\"nfsv3\":{\"port111\":" << (nfs3_portmapper ? L"true" : L"false")
                    << L",\"port2049\":" << (nfs3_port2049 ? L"true" : L"false")
                    << L",\"online\":" << (nfs3_online ? L"true" : L"false") << L"}"
                    << L",\"nfsv4\":{\"port2049\":" << (nfs4_port2049 ? L"true" : L"false")
                    << L",\"online\":" << (nfs4_online ? L"true" : L"false") << L"}"
                    << L"}";
            NfsOutputFormatter::PrintJsonEnvelope(L"showmount", status, payload.str());
            return status;
        }

        if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
            NfsOutputFormatter::PrintDelimitedRow({L"host", L"family", L"nfsMode", L"timeoutMs", L"nfsv3_port111", L"nfsv3_port2049", L"nfsv4_port2049", L"nfsv3_online", L"nfsv4_online"}, options.outputMode);
            NfsOutputFormatter::PrintDelimitedRow({remote_host,
                               NfsOutputFormatter::AddressFamilyLabel(options.addressFamily),
                               NfsOutputFormatter::NfsProbeModeLabel(options.nfsProbeMode),
                               std::to_wstring(options.timeoutMs),
                               nfs3_portmapper ? L"true" : L"false",
                               nfs3_port2049 ? L"true" : L"false",
                               nfs4_port2049 ? L"true" : L"false",
                               nfs3_online ? L"true" : L"false",
                               nfs4_online ? L"true" : L"false"},
                              options.outputMode);
            return status;
        }

        if (showV3) {
            NfsOutputFormatter::LogInfo(options, L"  NFSv3 Portmapper (TCP 111)  : %s\n", nfs3_portmapper ? L"ONLINE" : L"OFFLINE/BLOCKED");
            NfsOutputFormatter::LogInfo(options, L"  NFSv3 NFS        (TCP 2049) : %s\n", nfs3_port2049 ? L"ONLINE" : L"OFFLINE/BLOCKED");
        }
        if (showV4) {
            NfsOutputFormatter::LogInfo(options, L"  NFSv4 NFS        (TCP 2049) : %s\n", nfs4_port2049 ? L"ONLINE" : L"OFFLINE/BLOCKED");
        }
        NfsOutputFormatter::LogInfo(options, L"\n");

        return status;
    }
};

class NfsConfToolController {
public:
    static void PrintHelp() {
        wprintf(L"USAGE: nfsctl.exe --nfsconf --get <KeyName> [--json]\n"
                L"       nfsctl.exe --nfsconf --set <KeyName> <Value> [--confirm|--dry-run]\n");
    }

    static DWORD Execute(const std::vector<std::wstring>& args, const GlobalOptions& options) {
        ULONGLONG startTick = GetTickCount64();
        DWORD stateErr = NfsOutputFormatter::CheckOperationState(startTick, options);
        if (stateErr != ERROR_SUCCESS) return stateErr;

        std::wstring keyName = L"", setValStr = L"";
        BOOL mode_get = FALSE, mode_set = FALSE;

        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == L"--get" && i + 1 < args.size()) {
                mode_get = TRUE;
                keyName = args[++i];
            }
            if (args[i] == L"--set" && i + 2 < args.size()) {
                mode_set = TRUE;
                keyName = args[++i];
                setValStr = args[++i];
            }
        }

        const wchar_t* subKey = NfsRegistry::kClientDefaultSubKey;

        if (mode_get) {
            DWORD val = 0;
            DWORD dwErr = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, keyName.c_str(), &val);
            if (dwErr == ERROR_SUCCESS) {
                if (options.outputMode == OutputMode::Json) {
                    std::wstringstream payload;
                    payload << L"{\"key\":\"" << NfsOutputFormatter::JsonEscape(keyName) << L"\",\"value\":" << val << L"}";
                    NfsOutputFormatter::PrintJsonEnvelope(L"nfsconf.get", ERROR_SUCCESS, payload.str());
                } else if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
                    NfsOutputFormatter::PrintDelimitedRow({L"key", L"value"}, options.outputMode);
                    NfsOutputFormatter::PrintDelimitedRow({keyName, std::to_wstring(val)}, options.outputMode);
                } else {
                    NfsOutputFormatter::LogInfo(options, L"%s = %lu\n", keyName.c_str(), val);
                }
                return ERROR_SUCCESS;
            }
            if (options.outputMode == OutputMode::Json) {
                NfsOutputFormatter::PrintJsonEnvelope(L"nfsconf.get", dwErr, L"{\"key\":null,\"value\":null}",
                                  L"Registry key not found");
            } else {
                fwprintf(stderr, L"Error: Key '%s' not found under HKLM\\%s\n", keyName.c_str(), subKey);
            }
            return dwErr;
        }

        if (mode_set) {
            bool shouldExecute = false;
            std::wstring dryRunMessage = L"[dry-run] Would set HKLM\\" + std::wstring(subKey) + L"\\" + keyName + L" = " + setValStr + L"\n";
            DWORD prepErr = NfsRegistryEngine::PrepareWriteOperation(options, startTick, L"nfsconf --set", dryRunMessage, shouldExecute);
            if (prepErr != ERROR_SUCCESS || !shouldExecute) return prepErr;

            LONG dwordVal = 0;
            if (!NfsSecurityManager::SafeWstol(setValStr.c_str(), &dwordVal)) {
                fwprintf(stderr, L"Error: Value must be a valid integer.\n");
                return ERROR_INVALID_PARAMETER;
            }

            DWORD existingValue = 0;
            DWORD existingErr = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, keyName.c_str(), &existingValue);
            if (existingErr == ERROR_SUCCESS && existingValue != static_cast<DWORD>(dwordVal) && !options.forceWrite) {
                fwprintf(stderr, L"Error: Existing value differs (%lu). Use --force to overwrite.\n", existingValue);
                return ERROR_CANCELLED;
            }

            DWORD backupErr = NfsRegistryEngine::BackupRegistryValueIfRequested(subKey, keyName.c_str(), existingValue, existingErr, options);
            if (backupErr != ERROR_SUCCESS) return backupErr;

            DWORD dwErr = NfsRegistryEngine::WinRegSetDWORD(HKEY_LOCAL_MACHINE, subKey, keyName.c_str(), static_cast<DWORD>(dwordVal));
            if (dwErr == ERROR_SUCCESS) {
                if (options.outputMode == OutputMode::Json) {
                    std::wstringstream payload;
                    payload << L"{\"key\":\"" << NfsOutputFormatter::JsonEscape(keyName)
                            << L"\",\"oldValue\":" << ((existingErr == ERROR_SUCCESS) ? std::to_wstring(existingValue) : L"null")
                            << L",\"newValue\":" << dwordVal << L"}";
                    NfsOutputFormatter::PrintJsonEnvelope(L"nfsconf.set", ERROR_SUCCESS, payload.str());
                } else {
                    NfsOutputFormatter::LogInfo(options, L"[+] Successfully updated Registry: %s = %ld\n", keyName.c_str(), dwordVal);
                }
                return ERROR_SUCCESS;
            }
            NfsOutputFormatter::PrintWin32Error(L"WinRegSetDWORD failed", dwErr);
            return dwErr;
        }

        wprintf(L"Use '--get <KeyName>' or '--set <KeyName> <Value>' to modify ClientForNFS Registry.\n");
        return ERROR_SUCCESS;
    }
};

class NfsIdMapToolController {
public:
    static void PrintHelp() {
        wprintf(L"USAGE: nfsctl.exe --nfsidmap <AccountName>\n"
                L"Resolves a Windows account name to its SID.\n");
    }

    static DWORD Execute(const wchar_t* account_name, const GlobalOptions& options) {
        NfsIdMapResult resolved = NfsSecurityManager::ResolveAccountToSid(account_name);
        DWORD rc = resolved.code;
        if (options.outputMode == OutputMode::Json) {
            std::wstringstream payload;
            payload << L"{\"account\":\"" << NfsOutputFormatter::JsonEscape(resolved.account)
                    << L"\",\"resolved\":" << ((rc == ERROR_SUCCESS) ? L"true" : L"false")
                    << L",\"domain\":";
            if (rc == ERROR_SUCCESS) {
                payload << L"\"" << NfsOutputFormatter::JsonEscape(resolved.domain) << L"\"";
            } else {
                payload << L"null";
            }
            payload << L",\"sid\":";
            if (rc == ERROR_SUCCESS) {
                payload << L"\"" << NfsOutputFormatter::JsonEscape(resolved.sid) << L"\"";
            } else {
                payload << L"null";
            }
            payload << L"}";
            NfsOutputFormatter::PrintJsonEnvelope(L"nfsidmap", rc, payload.str());
        } else if (rc == ERROR_SUCCESS) {
            wprintf(L"Windows Account Resolution:\n");
            wprintf(L"  Account Name : %s\n", resolved.account.c_str());
            wprintf(L"  Domain       : %s\n", resolved.domain.c_str());
            wprintf(L"  SID          : %s\n", resolved.sid.c_str());
        } else {
            NfsOutputFormatter::PrintWin32Error(L"account resolution failed", rc);
        }
        return rc;
    }
};

class RpcDebugToolController {
public:
    static void PrintHelp() {
        wprintf(L"USAGE: nfsctl.exe --rpcdebug [--json]\n"
                L"       nfsctl.exe --rpcdebug --set <bitmask> [--confirm|--dry-run]\n");
    }

    static DWORD Execute(const std::vector<std::wstring>& args, const GlobalOptions& options) {
        ULONGLONG startTick = GetTickCount64();
        DWORD stateErr = NfsOutputFormatter::CheckOperationState(startTick, options);
        if (stateErr != ERROR_SUCCESS) return stateErr;

        std::wstring setValStr = L"";
        BOOL mode_set = FALSE;

        for (size_t i = 0; i < args.size(); ++i) {
            if ((args[i] == L"--set" || args[i] == L"-s") && i + 1 < args.size()) {
                mode_set = TRUE;
                setValStr = args[++i];
            }
        }

        const wchar_t* subKey = NfsRegistry::kClientDefaultSubKey;

        if (mode_set) {
            bool shouldExecute = false;
            std::wstring dryRunMessage = L"[dry-run] Would set ClientForNFS DebugFlags = " + setValStr + L"\n";
            DWORD prepErr = NfsRegistryEngine::PrepareWriteOperation(options, startTick, L"rpcdebug --set", dryRunMessage, shouldExecute);
            if (prepErr != ERROR_SUCCESS || !shouldExecute) return prepErr;

            LONG flags = 0;
            if (!NfsSecurityManager::SafeWstol(setValStr.c_str(), &flags)) {
                fwprintf(stderr, L"Error: Debug flag must be an integer bitmask.\n");
                return ERROR_INVALID_PARAMETER;
            }

            DWORD existingFlags = 0;
            DWORD existingErr = NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueDebugFlags, &existingFlags);
            if (existingErr == ERROR_SUCCESS && existingFlags != static_cast<DWORD>(flags) && !options.forceWrite) {
                fwprintf(stderr, L"Error: Existing DebugFlags differs (%lu). Use --force to overwrite.\n", existingFlags);
                return ERROR_CANCELLED;
            }

            DWORD backupErr = NfsRegistryEngine::BackupRegistryValueIfRequested(subKey, NfsRegistry::kValueDebugFlags, existingFlags, existingErr, options);
            if (backupErr != ERROR_SUCCESS) return backupErr;

            DWORD dwErr = NfsRegistryEngine::WinRegSetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueDebugFlags, static_cast<DWORD>(flags));
            if (dwErr == ERROR_SUCCESS) {
                if (options.outputMode == OutputMode::Json) {
                    std::wstringstream payload;
                    payload << L"{\"oldValue\":" << ((existingErr == ERROR_SUCCESS) ? std::to_wstring(existingFlags) : L"null")
                            << L",\"newValue\":" << flags << L"}";
                    NfsOutputFormatter::PrintJsonEnvelope(L"rpcdebug.set", ERROR_SUCCESS, payload.str());
                } else {
                    NfsOutputFormatter::LogInfo(options, L"[+] Set ClientForNFS DebugFlags = %ld\n", flags);
                }
                return ERROR_SUCCESS;
            }
            NfsOutputFormatter::PrintWin32Error(L"failed to set DebugFlags", dwErr);
            return dwErr;
        }

        DWORD debugFlags = 0;
        if (NfsRegistryEngine::WinRegGetDWORD(HKEY_LOCAL_MACHINE, subKey, NfsRegistry::kValueDebugFlags, &debugFlags) == ERROR_SUCCESS) {
            if (options.outputMode == OutputMode::Json) {
                std::wstringstream payload;
                payload << L"{\"debugFlags\":" << debugFlags << L"}";
                NfsOutputFormatter::PrintJsonEnvelope(L"rpcdebug.get", ERROR_SUCCESS, payload.str());
            } else if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
                NfsOutputFormatter::PrintDelimitedRow({L"debugFlags"}, options.outputMode);
                NfsOutputFormatter::PrintDelimitedRow({std::to_wstring(debugFlags)}, options.outputMode);
            } else {
                NfsOutputFormatter::LogInfo(options, L"ClientForNFS DebugFlags = %lu\n", debugFlags);
            }
        } else {
            if (options.outputMode == OutputMode::Json) {
                NfsOutputFormatter::PrintJsonEnvelope(L"rpcdebug.get", ERROR_SUCCESS, L"{\"debugFlags\":0}");
            } else if (options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) {
                NfsOutputFormatter::PrintDelimitedRow({L"debugFlags"}, options.outputMode);
                NfsOutputFormatter::PrintDelimitedRow({L"0"}, options.outputMode);
            } else {
                NfsOutputFormatter::LogInfo(options, L"ClientForNFS DebugFlags = 0 (Disabled / Default)\n");
            }
        }
        return ERROR_SUCCESS;
    }
};

// ============================================================================
// 6. HELP SYSTEM & CLI PARSER
// ============================================================================

class NfsctlHelpSystem {
public:
    static void PrintMainHelp() {
        wprintf(LR"(nfsctl(1)               CrossShell for UNIX Reference Manual                 nfsctl(1)

    NAME
        nfsctl - NFS client, mounts, exports, and RPC diagnostic control utility

    SYNOPSIS
        nfsctl TOOL_SWITCH [OPTIONS] [ARGUMENTS...]

    DESCRIPTION
        Manages Windows Client for NFS, active NFS network mounts, remote server
        exports, and RPC diagnostic configurations. Integrates with the Windows
        Networking API (WNet), Client for NFS Registry settings, and RPC portmapper
        subsystems to provide unified NFS administration.

    TOOL SWITCHES
        -m, --mounts
            Inspect active network and NFS drive mounts.

        --mount PATH DRIVE:
            Mount an NFS share (e.g. \\192.168.1.50\srv Z:).

        --umount DRIVE:
            Unmount a mapped network drive (e.g. Z:).

        -e, --exportfs
            Enumerate local Windows Server for NFS exports.

        -s, --showmount HOST
            Probe a remote NFS server for exports and mount points (NFSv3/NFSv4).

        -st, --nfsstat
            Query Windows Client for NFS service and Registry state.

        -c, --nfsconf
            Read or set Windows Client for NFS Registry configuration.

        -id, --nfsidmap USER
            Resolve Windows User Account to SID mapping.

        -d, --rpcdebug
            Read or set Client for NFS Registry DebugFlags.

    GLOBAL OPTIONS
        --output FORMAT
            Select table, csv, tsv, or json output. The default is table.

        --json, -j, --csv, --tsv, --table
            Convenience shortcuts for structured output formats.

        --pipe COMMAND
            Stream formatted output directly to another command or utility.

        --dry-run
            Preview write operations without applying registry changes.

        --confirm
            Confirm write operations that modify state.

        --force
            Allow overriding existing differing registry values.

        --backup-reg [PATH]
            Save pre-change registry values to a backup file.

        --timeout-ms MS
            Network probe timeout in milliseconds (default 1500).

        --ipv4, --ipv6, --dual-stack
            Select socket probe IP protocol family.

        -q, --quiet
            Suppress informational diagnostics.

        --verbose, --trace
            Display detailed execution diagnostics or trace-level logging.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        nfsctl --mounts
            List all active NFS and network drive mounts.

        nfsctl --showmount 192.168.1.50
            Probe exports on a remote NFS server.

        nfsctl --mount \\192.168.1.50\exports Z:
            Mount an NFS share to drive letter Z:.

        nfsctl --umount Z:
            Unmount network drive Z:.

        nfsctl --nfsstat --json
            Query Client for NFS status and emit JSON.

        nfsctl --showmount 192.168.1.50 --csv
            Export remote share list as CSV.

    CrossShell for UNIX                                                     nfsctl(1)
)");
    }
    static bool HasHelpFlag(const std::vector<std::wstring>& args) {
        for (const auto& arg : args) {
            if (arg == L"--help" || arg == L"-h") {
                return true;
            }
        }
        return false;
    }
};

class NfsctlOptionsParser {
public:
    static DWORD ParseGlobalOptions(std::vector<std::wstring>& args, GlobalOptions& options) {
        std::vector<std::wstring> filtered;
        for (size_t i = 0; i < args.size(); ++i) {
            const std::wstring& arg = args[i];
            if (arg == L"--json" || arg == L"-j") {
                options.outputMode = OutputMode::Json;
                continue;
            }
            if (arg == L"--csv") {
                options.outputMode = OutputMode::Csv;
                continue;
            }
            if (arg == L"--tsv") {
                options.outputMode = OutputMode::Tsv;
                continue;
            }
            if (arg == L"--dry-run") {
                options.dryRun = true;
                continue;
            }
            if (arg == L"--confirm") {
                options.confirm = true;
                continue;
            }
            if (arg == L"--force") {
                options.forceWrite = true;
                continue;
            }
            if (arg == L"--backup-reg") {
                options.backupReg = true;
                if (i + 1 < args.size() && !args[i + 1].empty() && args[i + 1][0] != L'-') {
                    options.backupRegPath = args[++i];
                }
                continue;
            }
            if (arg == L"--win32-exit") {
                options.win32Exit = true;
                continue;
            }
            if (arg == L"--quiet" || arg == L"-q") {
                options.quiet = true;
                continue;
            }
            if (arg == L"--verbose") {
                options.verbose = true;
                continue;
            }
            if (arg == L"--trace") {
                options.trace = true;
                options.verbose = true;
                continue;
            }
            if (arg == L"--ipv4") {
                options.addressFamily = AF_INET;
                continue;
            }
            if (arg == L"--ipv6") {
                options.addressFamily = AF_INET6;
                continue;
            }
            if (arg == L"--dual-stack") {
                options.addressFamily = AF_UNSPEC;
                continue;
            }
            if (arg == L"--nfsv3") {
                options.nfsProbeMode = NfsProbeMode::V3;
                continue;
            }
            if (arg == L"--nfsv4") {
                options.nfsProbeMode = NfsProbeMode::V4;
                continue;
            }
            if (arg == L"--nfsboth") {
                options.nfsProbeMode = NfsProbeMode::Both;
                continue;
            }
            if (arg == L"--timeout-ms") {
                if (i + 1 >= args.size()) {
                    fwprintf(stderr, L"Error: --timeout-ms requires a value.\n");
                    return ERROR_INVALID_PARAMETER;
                }
                LONG timeout = 0;
                if (!NfsSecurityManager::SafeWstol(args[++i].c_str(), &timeout) || timeout <= 0) {
                    fwprintf(stderr, L"Error: --timeout-ms expects a positive integer.\n");
                    return ERROR_INVALID_PARAMETER;
                }
                options.timeoutMs = static_cast<int>(timeout);
                continue;
            }
            if (arg == L"--op-timeout-ms") {
                if (i + 1 >= args.size()) {
                    fwprintf(stderr, L"Error: --op-timeout-ms requires a value.\n");
                    return ERROR_INVALID_PARAMETER;
                }
                LONG timeout = 0;
                if (!NfsSecurityManager::SafeWstol(args[++i].c_str(), &timeout) || timeout < 0) {
                    fwprintf(stderr, L"Error: --op-timeout-ms expects a non-negative integer.\n");
                    return ERROR_INVALID_PARAMETER;
                }
                options.opTimeoutMs = static_cast<int>(timeout);
                continue;
            }
            filtered.push_back(arg);
        }

        if (options.trace) {
            options.verbose = true;
        }

        args.swap(filtered);
        return ERROR_SUCCESS;
    }
};

// ============================================================================
// 7. APPLICATION LIFECYCLE CONTROLLER
// ============================================================================

class NfsctlApplication {
public:
    int Run(int argc, wchar_t* argv[]) {
        SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

        WinsockSubsystem winsock;
        if (!winsock.Ok()) {
            DWORD err = static_cast<DWORD>(winsock.Status());
            NfsOutputFormatter::PrintWin32Error(L"WSAStartup failed", err);
            return NfsOutputFormatter::NormalizeExitCode(err);
        }

        if (argc < 2) {
            NfsctlHelpSystem::PrintMainHelp();
            return ERROR_SUCCESS;
        }

        std::wstring main_switch = argv[1];
        std::vector<std::wstring> sub_args;
        for (int i = 2; i < argc; ++i) {
            sub_args.push_back(argv[i]);
        }

        GlobalOptions options;
        DWORD parseErr = NfsctlOptionsParser::ParseGlobalOptions(sub_args, options);
        if (parseErr != ERROR_SUCCESS) {
            return options.win32Exit ? static_cast<int>(parseErr) : NfsOutputFormatter::NormalizeExitCode(parseErr);
        }

        if ((options.outputMode == OutputMode::Csv || options.outputMode == OutputMode::Tsv) &&
            (main_switch == L"--mount" || main_switch == L"--umount" ||
             (main_switch == L"--nfsconf" && !NfsctlHelpSystem::HasHelpFlag(sub_args)) ||
             (main_switch == L"--rpcdebug" && !NfsctlHelpSystem::HasHelpFlag(sub_args)))) {
            fwprintf(stderr, L"Error: CSV/TSV output is only supported for read/query commands.\n");
            DWORD modeErr = ERROR_INVALID_PARAMETER;
            return options.win32Exit ? static_cast<int>(modeErr) : NfsOutputFormatter::NormalizeExitCode(modeErr);
        }

        if (main_switch == L"--help" || main_switch == L"-h") {
            NfsctlHelpSystem::PrintMainHelp();
            return ERROR_SUCCESS;
        }
        if (main_switch == L"--version" || main_switch == L"-v") {
            wprintf(L"NFSCTL v5.0.0\n");
            return ERROR_SUCCESS;
        }

        DWORD result = ERROR_INVALID_PARAMETER;

        if (main_switch == L"--mounts" || main_switch == L"-m" || 
            main_switch == L"--mount"  || main_switch == L"--umount") {
            std::vector<std::wstring> full_args;
            full_args.push_back(main_switch);
            full_args.insert(full_args.end(), sub_args.begin(), sub_args.end());
            if (NfsctlHelpSystem::HasHelpFlag(full_args)) {
                MountsToolController::PrintHelp();
                return ERROR_SUCCESS;
            }
            result = MountsToolController::Execute(full_args, options);
            return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
        }

        if (main_switch == L"--nfsstat" || main_switch == L"-st") {
            if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
                NfsStatToolController::PrintHelp();
                return ERROR_SUCCESS;
            }
            result = NfsStatToolController::Execute(options);
            return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
        }
        if (main_switch == L"--exportfs" || main_switch == L"-e") {
            if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
                ExportFsToolController::PrintHelp();
                return ERROR_SUCCESS;
            }
            result = ExportFsToolController::Execute(options);
            return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
        }
        if (main_switch == L"--showmount" || main_switch == L"-s") {
            if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
                ShowMountToolController::PrintHelp();
                return ERROR_SUCCESS;
            }
            if (sub_args.empty()) {
                fwprintf(stderr, L"Error: Host argument missing. Example: nfsctl.exe --showmount 192.168.1.50\n");
                result = ERROR_INVALID_PARAMETER;
                return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
            }
            result = ShowMountToolController::Execute(sub_args[0].c_str(), options);
            return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
        }
        if (main_switch == L"--nfsconf" || main_switch == L"-c") {
            if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
                NfsConfToolController::PrintHelp();
                return ERROR_SUCCESS;
            }
            result = NfsConfToolController::Execute(sub_args, options);
            return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
        }
        if (main_switch == L"--nfsidmap" || main_switch == L"-id") {
            if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
                NfsIdMapToolController::PrintHelp();
                return ERROR_SUCCESS;
            }
            if (sub_args.empty()) {
                fwprintf(stderr, L"Error: Account name missing. Example: nfsctl.exe --nfsidmap Administrator\n");
                result = ERROR_INVALID_PARAMETER;
                return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
            }
            result = NfsIdMapToolController::Execute(sub_args[0].c_str(), options);
            return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
        }
        if (main_switch == L"--rpcdebug" || main_switch == L"-d") {
            if (NfsctlHelpSystem::HasHelpFlag(sub_args)) {
                RpcDebugToolController::PrintHelp();
                return ERROR_SUCCESS;
            }
            result = RpcDebugToolController::Execute(sub_args, options);
            return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
        }

        fwprintf(stderr, L"Error: Unknown switch '%s'. Run 'nfsctl.exe --help'.\n", main_switch.c_str());
        result = ERROR_INVALID_PARAMETER;
        return options.win32Exit ? static_cast<int>(result) : NfsOutputFormatter::NormalizeExitCode(result);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    NfsctlApplication app;
    return app.Run(argc, argv);
}
