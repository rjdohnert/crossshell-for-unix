#include "nt_kernel_info_provider.hpp"
#include "system_details.hpp"

std::wstring NtKernelInfoProvider::queryRegString(HKEY hKey, const wchar_t* valueName) {
        if (!hKey) return L"";
        DWORD bytesNeeded = 0;
        DWORD type = 0;
        LONG status = RegQueryValueExW(hKey, valueName, nullptr, &type, nullptr, &bytesNeeded);
        if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ) || bytesNeeded == 0) {
            return L"";
        }
        std::vector<wchar_t> buffer((bytesNeeded / sizeof(wchar_t)) + 1, 0);
        status = RegQueryValueExW(hKey, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer.data()), &bytesNeeded);
        return (status == ERROR_SUCCESS) ? std::wstring(buffer.data()) : L"";
    }

DWORD NtKernelInfoProvider::queryRegDword(HKEY hKey, const wchar_t* valueName, DWORD defaultVal ) {
        if (!hKey) return defaultVal;
        DWORD val = 0;
        DWORD size = sizeof(val);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS && type == REG_DWORD) {
            return val;
        }
        return defaultVal;
    }

std::wstring NtKernelInfoProvider::detectEdition(DWORD major, DWORD minor, DWORD build, bool isServer, const std::wstring& regProductName) {
        if (isServer) {
            if (build >= 26100) return L"Windows Server 2025";
            if (build >= 20348) return L"Windows Server 2022";
            if (build >= 17763) return L"Windows Server 2019";
            if (build >= 14393) return L"Windows Server 2016";
            if (build >= 9600)  return L"Windows Server 2012 R2";
            return regProductName.empty() ? L"Windows Server" : regProductName;
        }

        if (major == 10) {
            if (build >= 22000) {
                std::wstring prod = regProductName;
                size_t pos = prod.find(L"Windows 10");
                if (pos != std::wstring::npos) {
                    prod.replace(pos, 10, L"Windows 11");
                }
                return prod.empty() ? L"Windows 11" : prod;
            }
            return regProductName.empty() ? L"Windows 10" : regProductName;
        }

        return regProductName.empty() ? L"Windows NT" : regProductName;
    }

SystemDetails NtKernelInfoProvider::collect() {
        SystemDetails sys;
        sys.sysname = L"Windows NT";

        wchar_t nodeBuf[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
        DWORD nodeSize = sizeof(nodeBuf) / sizeof(nodeBuf[0]);
        if (GetComputerNameExW(ComputerNamePhysicalDnsHostname, nodeBuf, &nodeSize)) {
            sys.nodename = nodeBuf;
        } else {
            sys.nodename = L"localhost";
        }

        DWORD major = KUSD_MAJOR_VERSION;
        DWORD minor = KUSD_MINOR_VERSION;
        DWORD build = KUSD_BUILD_NUMBER;

        if (major == 0) {
            RTL_OSVERSIONINFOW osInfo = { 0 };
            osInfo.dwOSVersionInfoSize = sizeof(osInfo);
            if (RtlGetVersion(&osInfo) == 0) {
                major = osInfo.dwMajorVersion;
                minor = osInfo.dwMinorVersion;
                build = osInfo.dwBuildNumber;
            }
        }

        sys.buildNumber = build;

        HKEY hKeyCurrentVersion = nullptr;
        std::wstring regProductName;
        std::wstring displayVersion;
        std::wstring installType;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
                          0, KEY_READ, &hKeyCurrentVersion) == ERROR_SUCCESS) {
            sys.ubr = queryRegDword(hKeyCurrentVersion, L"UBR", 0);
            installType = queryRegString(hKeyCurrentVersion, L"InstallationType");
            regProductName = queryRegString(hKeyCurrentVersion, L"ProductName");
            displayVersion = queryRegString(hKeyCurrentVersion, L"DisplayVersion");
            if (displayVersion.empty()) {
                displayVersion = queryRegString(hKeyCurrentVersion, L"ReleaseId");
            }
            sys.licenseId = queryRegString(hKeyCurrentVersion, L"ProductId");
            RegCloseKey(hKeyCurrentVersion);
        }

        bool isServer = (_wcsicmp(installType.c_str(), L"Server") == 0);
        sys.marketingEdition = detectEdition(major, minor, build, isServer, regProductName);

        std::wstringstream relStream;
        relStream << major << L"." << minor;
        if (!displayVersion.empty()) {
            relStream << L"." << displayVersion;
        }
        sys.release = relStream.str();

        std::wstringstream verStream;
        verStream << L"Build " << build;
        if (sys.ubr > 0) {
            verStream << L"." << sys.ubr;
        }
        sys.version = verStream.str();

        SYSTEM_INFO si;
        GetNativeSystemInfo(&si);
        sys.numProcessors = si.dwNumberOfProcessors;

        switch (si.wProcessorArchitecture) {
            case PROCESSOR_ARCHITECTURE_AMD64: sys.machine = L"x86_64"; break;
            case PROCESSOR_ARCHITECTURE_ARM64: sys.machine = L"arm64"; break;
            case PROCESSOR_ARCHITECTURE_ARM:   sys.machine = L"arm"; break;
            case PROCESSOR_ARCHITECTURE_INTEL: sys.machine = L"i686"; break;
            default:                           sys.machine = L"unknown"; break;
        }

        HKEY hKeyCrypto = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 
                          0, KEY_READ, &hKeyCrypto) == ERROR_SUCCESS) {
            sys.machineId = queryRegString(hKeyCrypto, L"MachineGuid");
            RegCloseKey(hKeyCrypto);
        }

        HKEY hKeyBios = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", 
                          0, KEY_READ, &hKeyBios) == ERROR_SUCCESS) {
            std::wstring mfg = queryRegString(hKeyBios, L"SystemManufacturer");
            std::wstring prod = queryRegString(hKeyBios, L"SystemProductName");
            if (!mfg.empty() || !prod.empty()) {
                sys.model = mfg + (mfg.empty() || prod.empty() ? L"" : L" ") + prod;
            } else {
                sys.model = L"PC-Compatible Hardware";
            }
            RegCloseKey(hKeyBios);
        } else {
            sys.model = L"PC-Compatible Hardware";
        }

        return sys;
    }
