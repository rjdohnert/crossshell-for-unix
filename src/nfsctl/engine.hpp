#ifndef NFSCTL_ENGINE_HPP
#define NFSCTL_ENGINE_HPP

#include "nfsctl.hpp"
#include "reporter.hpp"

class NfsSecurityManager {
public:
    static BOOL IsAdminElevated();
    static DWORD RequireAdmin(const wchar_t* operation);
    static BOOL SafeWstol(const wchar_t* str, LONG* outVal);
    static NfsIdMapResult ResolveAccountToSid(const wchar_t* account_name);
};

class NfsRegistryEngine {
public:
    static DWORD WinRegGetDWORD(HKEY hKeyRoot, const wchar_t* subKey, const wchar_t* valueName, DWORD* outValue);
    static DWORD WinRegSetDWORD(HKEY hKeyRoot, const wchar_t* subKey, const wchar_t* valueName, DWORD value);
    static DWORD WinRegEnumSubkeys(HKEY hKeyRoot, const wchar_t* subKey, std::vector<std::wstring>& subkeys);
    static DWORD BackupRegistryValue(const wchar_t* subKey, const wchar_t* valueName, DWORD existingVal, DWORD readErr,
                                     const GlobalOptions& options, std::wstring& outPath);
    static DWORD BackupRegistryValueIfRequested(const wchar_t* subKey, const wchar_t* valueName, DWORD existingVal,
                                                DWORD readErr, const GlobalOptions& options);
    static DWORD PrepareWriteOperation(const GlobalOptions& options, ULONGLONG startTick, const wchar_t* operation,
                                       const std::wstring& dryRunMessage, bool& shouldExecute, bool requireAdmin = true);
};

class NfsMountManager {
public:
    static bool IsLikelyNfsMount(const WinNfsMountInfo& mount);
    static bool IsNfsClientAvailable();
    static DWORD EnumWindowsNetworkMounts(std::vector<WinNfsMountInfo>& mounts, const GlobalOptions& options, ULONGLONG startTick);
    static DWORD WinMountNfsShare(const wchar_t* remotePath, const wchar_t* localDrive);
    static DWORD WinUnmountNfsShare(const wchar_t* localDrive);
};

class NfsProbeEngine {
public:
    static BOOL ProbeTcpPort(const wchar_t* host, int port, int timeout_ms, int address_family);
};

#endif // NFSCTL_ENGINE_HPP
