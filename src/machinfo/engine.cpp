#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <wbemidl.h>
#include <tbs.h>
#include <wrl/client.h>

#include "engine.hpp"
#include "reporter.hpp"
#include <vector>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "tbs.lib")
#pragma comment(lib, "Advapi32.lib")

using Microsoft::WRL::ComPtr;

// ============================================================================
// SmbiosParser
// ============================================================================

std::wstring SmbiosParser::getSmbiosString(const BYTE* headerPtr, BYTE stringIndex, const BYTE* endPtr) {
    if (stringIndex == 0 || headerPtr == nullptr || endPtr == nullptr || headerPtr >= endPtr) {
        return L"N/A";
    }

    if (static_cast<size_t>(endPtr - headerPtr) < sizeof(SMBIOSHeader)) {
        return L"N/A";
    }

    const SMBIOSHeader* header = reinterpret_cast<const SMBIOSHeader*>(headerPtr);
    if (header->Length < sizeof(SMBIOSHeader) || static_cast<size_t>(endPtr - headerPtr) < header->Length) {
        return L"N/A";
    }

    const BYTE* strPtr = headerPtr + header->Length;
    if (strPtr >= endPtr) {
        return L"N/A";
    }

    BYTE currentIndex = 1;
    while (strPtr < endPtr) {
        const BYTE* current = strPtr;
        while (current < endPtr && *current != 0) {
            ++current;
        }
        if (current >= endPtr) break;

        if (currentIndex == stringIndex) {
            const int len = MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(strPtr),
                                                static_cast<int>(current - strPtr), nullptr, 0);
            if (len > 0) {
                std::vector<wchar_t> wbuf(len);
                MultiByteToWideChar(CP_ACP, 0, reinterpret_cast<const char*>(strPtr),
                                    static_cast<int>(current - strPtr), wbuf.data(), len);
                return std::wstring(wbuf.data(), static_cast<size_t>(len));
            }
            return L"N/A";
        }

        strPtr = current + 1;
        ++currentIndex;
        if (strPtr >= endPtr) break;
    }
    return L"N/A";
}

SystemFirmwareInfo SmbiosParser::parseFirmware() {
    SystemFirmwareInfo fw;

    FIRMWARE_TYPE fwType;
    if (GetFirmwareType(&fwType)) {
        if (fwType == FirmwareTypeUefi) fw.firmwareType = L"UEFI";
        else if (fwType == FirmwareTypeBios) fw.firmwareType = L"Legacy BIOS";
    }

    const DWORD smbiosSignature = 0x52534D42; // 'RSMB'
    DWORD size = GetSystemFirmwareTable(smbiosSignature, 0, nullptr, 0);
    if (size == 0) return fw;

    std::vector<BYTE> buffer(size);
    if (GetSystemFirmwareTable(smbiosSignature, 0, buffer.data(), size) == 0) return fw;
    if (size < sizeof(RawSMBIOSData) - 1) return fw;

    const RawSMBIOSData* smbios = reinterpret_cast<const RawSMBIOSData*>(buffer.data());
    if (smbios->Length == 0) return fw;

    const BYTE* tableStart = smbios->SMBIOSTableData;
    const BYTE* bufferEnd = buffer.data() + size;
    if (tableStart < buffer.data() || tableStart > bufferEnd) return fw;

    size_t declaredTableBytes = static_cast<size_t>(smbios->Length);
    size_t availableBytes = static_cast<size_t>(bufferEnd - tableStart);
    if (declaredTableBytes > availableBytes) {
        declaredTableBytes = availableBytes;
    }

    const BYTE* endPtr = tableStart + declaredTableBytes;
    size_t offset = 0;

    while (offset < declaredTableBytes) {
        const size_t remaining = declaredTableBytes - offset;
        if (remaining < sizeof(SMBIOSHeader)) break;

        const BYTE* ptr = tableStart + offset;
        const SMBIOSHeader* header = reinterpret_cast<const SMBIOSHeader*>(ptr);
        if (header->Length < sizeof(SMBIOSHeader) || header->Length > remaining) break;

        // Type 0: BIOS Information
        if (header->Type == 0 && header->Length >= 0x12) {
            fw.biosVendor = getSmbiosString(ptr, ptr[0x04], endPtr);
            fw.biosVersion = getSmbiosString(ptr, ptr[0x05], endPtr);
            fw.biosReleaseDate = getSmbiosString(ptr, ptr[0x08], endPtr);
        }
        // Type 1: System Information
        else if (header->Type == 1 && header->Length >= 0x19) {
            fw.manufacturer = getSmbiosString(ptr, ptr[0x04], endPtr);
            fw.productName = getSmbiosString(ptr, ptr[0x05], endPtr);
            fw.serialNumber = getSmbiosString(ptr, ptr[0x07], endPtr);

            wchar_t uuidBuf[64] = { 0 };
            const BYTE* u = ptr + 0x08;
            if (u + 16 <= endPtr) {
                swprintf_s(uuidBuf, 64, L"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                           u[3], u[2], u[1], u[0], u[5], u[4], u[7], u[6],
                           u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
                fw.uuid = uuidBuf;
            }
        }

        size_t next = offset + header->Length;
        while (next + 1 < declaredTableBytes &&
               (tableStart[next] != 0 || tableStart[next + 1] != 0)) {
            ++next;
        }

        if (next + 1 < declaredTableBytes) {
            offset = next + 2;
        } else {
            break;
        }
    }

    return fw;
}

// ============================================================================
// RegistryHelper
// ============================================================================

bool RegistryHelper::readString(HKEY hRoot, const wchar_t* subkeyPath, const wchar_t* valueName, std::wstring& outValue) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(hRoot, subkeyPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD dataType = 0;
    DWORD dataSize = 0;
    LONG rc = RegQueryValueExW(hKey, valueName, nullptr, &dataType, nullptr, &dataSize);
    if (rc != ERROR_SUCCESS || dataSize == 0 || (dataType != REG_SZ && dataType != REG_EXPAND_SZ)) {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<wchar_t> buffer((dataSize / sizeof(wchar_t)) + 1, L'\0');
    rc = RegQueryValueExW(hKey, valueName, nullptr, &dataType, reinterpret_cast<LPBYTE>(buffer.data()), &dataSize);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS) {
        return false;
    }

    outValue = std::wstring(buffer.data());
    return !outValue.empty();
}

bool RegistryHelper::readDword(HKEY hRoot, const wchar_t* subkeyPath, const wchar_t* valueName, DWORD& outValue) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(hRoot, subkeyPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD dataType = 0;
    DWORD dataSize = sizeof(DWORD);
    DWORD value = 0;
    LONG rc = RegQueryValueExW(hKey, valueName, nullptr, &dataType, reinterpret_cast<LPBYTE>(&value), &dataSize);
    RegCloseKey(hKey);
    if (rc != ERROR_SUCCESS || dataType != REG_DWORD || dataSize != sizeof(DWORD)) {
        return false;
    }

    outValue = value;
    return true;
}

// ============================================================================
// TpmDiagnosticsProvider
// ============================================================================

std::wstring TpmDiagnosticsProvider::manufacturerIdToString(DWORD id) {
    wchar_t out[5] = {
        static_cast<wchar_t>((id >> 24) & 0xFF),
        static_cast<wchar_t>((id >> 16) & 0xFF),
        static_cast<wchar_t>((id >> 8) & 0xFF),
        static_cast<wchar_t>(id & 0xFF),
        L'\0'
    };

    for (int i = 0; i < 4; ++i) {
        if (out[i] < 32 || out[i] > 126) {
            return L"N/A";
        }
    }
    return std::wstring(out);
}

TpmInfo TpmDiagnosticsProvider::query(bool enableTpmWmi) {
    TpmInfo tpm;

    HANDLE hTpm = CreateFileW(L"\\\\.\\TPM", 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hTpm != INVALID_HANDLE_VALUE) {
        tpm.present = true;
        tpm.ready = true;
        CloseHandle(hTpm);
    } else {
        const DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION) {
            tpm.present = true;
            tpm.ready = true;
        }
    }

    const wchar_t* tpmRegPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\TPM";
    std::wstring value;
    if (RegistryHelper::readString(HKEY_LOCAL_MACHINE, tpmRegPath, L"SpecVersion", value)) {
        tpm.specVersion = value;
        tpm.present = true;
    }
    if (RegistryHelper::readString(HKEY_LOCAL_MACHINE, tpmRegPath, L"ManufacturerName", value)) {
        tpm.manufacturer = value;
        tpm.present = true;
    }
    if (RegistryHelper::readString(HKEY_LOCAL_MACHINE, tpmRegPath, L"ManufacturerVersion", value)) {
        tpm.manufacturerVersion = value;
        tpm.present = true;
    }

    TPM_DEVICE_INFO tbsInfo = {};
    tbsInfo.structVersion = 1;
    if (Tbsi_GetDeviceInfo(sizeof(tbsInfo), &tbsInfo) == TBS_SUCCESS) {
        tpm.present = true;
        tpm.ready = true;
        if (tpm.specVersion == L"N/A") {
            if (tbsInfo.tpmVersion == TPM_VERSION_20) tpm.specVersion = L"2.0";
            else if (tbsInfo.tpmVersion == TPM_VERSION_12) tpm.specVersion = L"1.2";
        }
    }

    if (tpm.manufacturer == L"N/A") {
        DWORD taskManufacturerId = 0;
        if (RegistryHelper::readDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\TPM\\WMI", L"TaskManufacturerId", taskManufacturerId)) {
            const std::wstring maker = manufacturerIdToString(taskManufacturerId);
            if (maker != L"N/A") {
                tpm.manufacturer = maker;
                tpm.present = true;
            }
        }
    }

    if (!enableTpmWmi) {
        return tpm;
    }

    bool comInitialized = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) {
        comInitialized = true;
    } else if (hr != RPC_E_CHANGED_MODE) {
        return tpm;
    }

    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr,
                              RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE,
                              nullptr, EOAC_NONE, nullptr);
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        if (comInitialized) CoUninitialize();
        return tpm;
    }

    ComPtr<IWbemLocator> pLocator;
    hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, reinterpret_cast<void**>(pLocator.GetAddressOf()));
    if (FAILED(hr) || !pLocator) {
        if (comInitialized) CoUninitialize();
        return tpm;
    }

    ComPtr<IWbemServices> pServices;
    BSTR ns = SysAllocString(L"ROOT\\CIMV2\\Security\\MicrosoftTpm");
    if (!ns) {
        if (comInitialized) CoUninitialize();
        return tpm;
    }
    hr = pLocator->ConnectServer(ns, nullptr, nullptr, nullptr, 0, nullptr, nullptr, pServices.GetAddressOf());
    SysFreeString(ns);
    if (FAILED(hr) || !pServices) {
        if (comInitialized) CoUninitialize();
        return tpm;
    }

    hr = CoSetProxyBlanket(pServices.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                           RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    if (FAILED(hr)) {
        if (comInitialized) CoUninitialize();
        return tpm;
    }

    ComPtr<IEnumWbemClassObject> pEnumerator;
    BSTR queryLanguage = SysAllocString(L"WQL");
    BSTR queryText = SysAllocString(L"SELECT SpecVersion, ManufacturerIdTxt, ManufacturerVersion, IsEnabled_InitialValue, IsActivated_InitialValue FROM Win32_Tpm");
    if (!queryLanguage || !queryText) {
        if (queryLanguage) SysFreeString(queryLanguage);
        if (queryText) SysFreeString(queryText);
        if (comInitialized) CoUninitialize();
        return tpm;
    }

    hr = pServices->ExecQuery(queryLanguage, queryText, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, pEnumerator.GetAddressOf());
    SysFreeString(queryLanguage);
    SysFreeString(queryText);

    if (SUCCEEDED(hr) && pEnumerator) {
        ComPtr<IWbemClassObject> pObj;
        ULONG returned = 0;
        if (pEnumerator->Next(WBEM_INFINITE, 1, pObj.GetAddressOf(), &returned) == WBEM_S_NO_ERROR && returned == 1 && pObj) {
            tpm.present = true;
            VARIANT vt;
            VariantInit(&vt);

            if (SUCCEEDED(pObj->Get(L"SpecVersion", 0, &vt, nullptr, nullptr)) && vt.vt == VT_BSTR && vt.bstrVal && tpm.specVersion == L"N/A") {
                tpm.specVersion = vt.bstrVal;
            }
            VariantClear(&vt);

            VariantInit(&vt);
            if (SUCCEEDED(pObj->Get(L"ManufacturerIdTxt", 0, &vt, nullptr, nullptr)) && vt.vt == VT_BSTR && vt.bstrVal && tpm.manufacturer == L"N/A") {
                tpm.manufacturer = vt.bstrVal;
            }
            VariantClear(&vt);

            VariantInit(&vt);
            if (SUCCEEDED(pObj->Get(L"ManufacturerVersion", 0, &vt, nullptr, nullptr)) && vt.vt == VT_BSTR && vt.bstrVal && tpm.manufacturerVersion == L"N/A") {
                tpm.manufacturerVersion = vt.bstrVal;
            }
            VariantClear(&vt);

            bool isEnabled = false;
            bool isActivated = false;

            VariantInit(&vt);
            if (SUCCEEDED(pObj->Get(L"IsEnabled_InitialValue", 0, &vt, nullptr, nullptr)) && vt.vt == VT_BOOL) {
                isEnabled = (vt.boolVal == VARIANT_TRUE);
            }
            VariantClear(&vt);

            VariantInit(&vt);
            if (SUCCEEDED(pObj->Get(L"IsActivated_InitialValue", 0, &vt, nullptr, nullptr)) && vt.vt == VT_BOOL) {
                isActivated = (vt.boolVal == VARIANT_TRUE);
            }
            VariantClear(&vt);

            tpm.ready = tpm.ready || (isEnabled && isActivated);
        }
    }

    if (comInitialized) CoUninitialize();
    return tpm;
}

// ============================================================================
// HardwareDiagnosticsScanner
// ============================================================================

bool HardwareDiagnosticsScanner::checkAVX2Support() {
    int cpuInfo[4] = { 0, 0, 0, 0 };
    __cpuid(cpuInfo, 0);
    const int maxId = cpuInfo[0];
    if (maxId < 7) return false;

    __cpuid(cpuInfo, 1);
    const bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
    const bool avx = (cpuInfo[2] & (1 << 28)) != 0;
    if (!osxsave || !avx) return false;

    const unsigned long long xcr0 = _xgetbv(0);
    if ((xcr0 & 0x6) != 0x6) return false;

    __cpuidex(cpuInfo, 7, 0);
    return (cpuInfo[1] & (1 << 5)) != 0;
}

CpuTopology HardwareDiagnosticsScanner::queryCpuTopology() {
    CpuTopology cpu;

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t nameBuf[256] = { 0 };
        DWORD dataSize = sizeof(nameBuf);
        if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr, reinterpret_cast<LPBYTE>(nameBuf), &dataSize) == ERROR_SUCCESS) {
            cpu.name = nameBuf;
        }
        DWORD mhz = 0;
        dataSize = sizeof(mhz);
        if (RegQueryValueExW(hKey, L"~MHZ", nullptr, nullptr, reinterpret_cast<LPBYTE>(&mhz), &dataSize) == ERROR_SUCCESS) {
            cpu.clockMhz = mhz;
        }
        RegCloseKey(hKey);
    }

    DWORD bufferSize = 0;
    GetLogicalProcessorInformationEx(RelationAll, nullptr, &bufferSize);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && bufferSize > 0) {
        std::vector<BYTE> buffer(bufferSize);
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());

        if (GetLogicalProcessorInformationEx(RelationAll, info, &bufferSize)) {
            DWORD bytesProcessed = 0;
            while (bytesProcessed < bufferSize) {
                if (info->Size == 0 || bytesProcessed + info->Size > bufferSize) break;
                if (info->Relationship == RelationProcessorPackage) {
                    cpu.socketCount++;
                } else if (info->Relationship == RelationProcessorCore) {
                    cpu.coreCount++;
                } else if (info->Relationship == RelationNumaNode) {
                    cpu.numaNodeCount++;
                } else if (info->Relationship == RelationCache) {
                    if (info->Cache.Level == 1) cpu.l1CacheBytes += info->Cache.CacheSize;
                    else if (info->Cache.Level == 2) cpu.l2CacheBytes += info->Cache.CacheSize;
                    else if (info->Cache.Level == 3) cpu.l3CacheBytes += info->Cache.CacheSize;
                }
                bytesProcessed += info->Size;
                info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(reinterpret_cast<BYTE*>(info) + info->Size);
            }
        }
    }

    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    cpu.logicalCount = sysInfo.dwNumberOfProcessors;
    cpu.supportsVirtualization = IsProcessorFeaturePresent(PF_VIRT_FIRMWARE_ENABLED) != 0;
    cpu.supportsAVX2 = checkAVX2Support();

    return cpu;
}

MemoryStatusInfo HardwareDiagnosticsScanner::queryMemoryStatus() {
    MemoryStatusInfo mem;
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);

    if (GlobalMemoryStatusEx(&statex)) {
        mem.totalPhysBytes = statex.ullTotalPhys;
        mem.availPhysBytes = statex.ullAvailPhys;
        mem.pageFileLimitBytes = statex.ullTotalPageFile;
        mem.loadPercentage = static_cast<double>(statex.dwMemoryLoad);
    }
    return mem;
}

typedef LONG (WINAPI* pfnRtlGetVersion)(OSVERSIONINFOEXW*);

std::wstring HardwareDiagnosticsScanner::queryWindowsVersion() {
    OSVERSIONINFOEXW osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

    HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
    if (hNtdll) {
        pfnRtlGetVersion pRtlGetVersion = reinterpret_cast<pfnRtlGetVersion>(GetProcAddress(hNtdll, "RtlGetVersion"));
        if (pRtlGetVersion) {
            pRtlGetVersion(&osvi);
        }
    }

    wchar_t buf[128];
    swprintf_s(buf, 128, L"Microsoft Windows %lu.%lu (Build %lu)",
               osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
    return std::wstring(buf);
}

// ============================================================================
// MachinfoEngine
// ============================================================================

MachinfoEngine::MachinfoEngine(MachinfoOptions opts) : options(std::move(opts)) {}

int MachinfoEngine::execute() {
    const bool enableTpmWmi = options.tpmWmi || (GetEnvironmentVariableW(L"MACHINFO_ENABLE_TPM_WMI", nullptr, 0) > 0);

    CpuTopology cpu = HardwareDiagnosticsScanner::queryCpuTopology();
    SystemFirmwareInfo fw = SmbiosParser::parseFirmware();
    MemoryStatusInfo mem = HardwareDiagnosticsScanner::queryMemoryStatus();
    TpmInfo tpm = TpmDiagnosticsProvider::query(enableTpmWmi);
    std::wstring osVersion = HardwareDiagnosticsScanner::queryWindowsVersion();

    MachinfoReporter::report(cpu, fw, mem, tpm, osVersion, options);
    return 0;
}
