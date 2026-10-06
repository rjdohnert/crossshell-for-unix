#include "engine.hpp"

// --- NfsSecurityManager Implementation ---
BOOL NfsSecurityManager::IsAdminElevated() {
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

DWORD NfsSecurityManager::RequireAdmin(const wchar_t* operation) {
    if (!IsAdminElevated()) {
        fwprintf(stderr, L"Error: Operation '%s' requires elevated Administrator privileges.\n", operation);
        return ERROR_ACCESS_DENIED;
    }
    return ERROR_SUCCESS;
}

BOOL NfsSecurityManager::SafeWstol(const wchar_t* str, LONG* outVal) {
    if (!str || *str == L'\0') return FALSE;
    wchar_t* endPtr = NULL;
    LONG val = wcstol(str, &endPtr, 10);
    if (endPtr == str || *endPtr != L'\0') return FALSE;
    *outVal = val;
    return TRUE;
}

NfsIdMapResult NfsSecurityManager::ResolveAccountToSid(const wchar_t* account_name) {
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

// --- NfsRegistryEngine Implementation ---
DWORD NfsRegistryEngine::WinRegGetDWORD(HKEY hKeyRoot, const wchar_t* subKey, const wchar_t* valueName, DWORD* outValue) {
    ScopedRegistryKey hKey;
    DWORD dwErr = RegOpenKeyExW(hKeyRoot, subKey, 0, KEY_READ, hKey.Receive());
    if (dwErr != ERROR_SUCCESS) return dwErr;

    DWORD type = 0;
    DWORD size = sizeof(DWORD);
    dwErr = RegQueryValueExW(hKey.Get(), valueName, NULL, &type, reinterpret_cast<LPBYTE>(outValue), &size);
    return (type == REG_DWORD) ? dwErr : ERROR_INVALID_DATATYPE;
}

DWORD NfsRegistryEngine::WinRegSetDWORD(HKEY hKeyRoot, const wchar_t* subKey, const wchar_t* valueName, DWORD value) {
    ScopedRegistryKey hKey;
    DWORD dwErr = RegCreateKeyExW(hKeyRoot, subKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, hKey.Receive(), NULL);
    if (dwErr != ERROR_SUCCESS) return dwErr;

    dwErr = RegSetValueExW(hKey.Get(), valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
    return dwErr;
}

DWORD NfsRegistryEngine::WinRegEnumSubkeys(HKEY hKeyRoot, const wchar_t* subKey, std::vector<std::wstring>& subkeys) {
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

DWORD NfsRegistryEngine::BackupRegistryValue(const wchar_t* subKey, const wchar_t* valueName, DWORD existingVal, DWORD readErr,
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

DWORD NfsRegistryEngine::BackupRegistryValueIfRequested(const wchar_t* subKey, const wchar_t* valueName, DWORD existingVal,
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

DWORD NfsRegistryEngine::PrepareWriteOperation(const GlobalOptions& options, ULONGLONG startTick, const wchar_t* operation,
                                               const std::wstring& dryRunMessage, bool& shouldExecute, bool requireAdmin) {
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

// --- NfsMountManager Implementation ---
bool NfsMountManager::IsLikelyNfsMount(const WinNfsMountInfo& mount) {
    if (mount.remotePath.empty()) return false;
    if (NfsOutputFormatter::ContainsIcase(mount.providerName, L"nfs")) return true;
    if (NfsOutputFormatter::ContainsIcase(mount.providerName, L"client for nfs")) return true;
    if (NfsOutputFormatter::ContainsIcase(mount.providerName, L"microsoft windows network")) return false;
    return false;
}

bool NfsMountManager::IsNfsClientAvailable() {
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

DWORD NfsMountManager::EnumWindowsNetworkMounts(std::vector<WinNfsMountInfo>& mounts, const GlobalOptions& options, ULONGLONG startTick) {
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

DWORD NfsMountManager::WinMountNfsShare(const wchar_t* remotePath, const wchar_t* localDrive) {
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

DWORD NfsMountManager::WinUnmountNfsShare(const wchar_t* localDrive) {
    DWORD dwErr = WNetCancelConnection2W(localDrive, CONNECT_UPDATE_PROFILE, TRUE);
    if (dwErr != NO_ERROR) {
        NfsOutputFormatter::PrintWin32Error(L"WNetCancelConnection2W failed", dwErr);
    }
    return dwErr;
}

// --- NfsProbeEngine Implementation ---
BOOL NfsProbeEngine::ProbeTcpPort(const wchar_t* host, int port, int timeout_ms, int address_family) {
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
