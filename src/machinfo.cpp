/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

/**
 * ============================================================================
 * SINGLE FILE INDEX: machinfo.cpp
 * ============================================================================
 *
 * TABLE OF CONTENTS:
 * 1. [DATA STRUCTURES & OPTIONS] ........... CpuTopology, Firmware, Memory, TpmInfo, MachinfoOptions
 * 2. [SMBIOS & REGISTRY PARSERS] ........... SmbiosParser, RegistryHelper classes
 * 3. [WMI & TPM DIAGNOSTICS PROVIDER] ...... WmiDiagnosticsProvider, TpmDiagnosticsProvider
 * 4. [HARDWARE SCANNER ENGINE] ............. HardwareDiagnosticsScanner class
 * 5. [STRUCTURED OUTPUT REPORTER] .......... MachinfoReporter class (Full, Quiet, Verbose, CSV, JSON)
 * 6. [APPLICATION CONTROLLER] .............. MachinfoApp class and wmain entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <intrin.h>
#include <wbemidl.h>
#include <tbs.h>
#include <wrl/client.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "tbs.lib")
#pragma comment(lib, "Advapi32.lib")

using Microsoft::WRL::ComPtr;

// ============================================================================
// 1. DATA STRUCTURES & OPTIONS
// ============================================================================

#pragma pack(push, 1)
struct RawSMBIOSData {
    BYTE Used20CallingMethod;
    BYTE MajorVersion;
    BYTE MinorVersion;
    BYTE DmiRevision;
    DWORD Length;
    BYTE SMBIOSTableData[1];
};

struct SMBIOSHeader {
    BYTE Type;
    BYTE Length;
    WORD Handle;
};
#pragma pack(pop)

struct CpuTopology {
    std::wstring name = L"Unknown Processor";
    DWORD clockMhz{0};
    DWORD socketCount{0};
    DWORD coreCount{0};
    DWORD logicalCount{0};
    DWORD numaNodeCount{0};
    ULONGLONG l1CacheBytes{0};
    ULONGLONG l2CacheBytes{0};
    ULONGLONG l3CacheBytes{0};
    bool is64Bit{true};
    bool supportsVirtualization{false};
    bool supportsAVX2{false};
};

struct SystemFirmwareInfo {
    std::wstring manufacturer = L"N/A";
    std::wstring productName = L"N/A";
    std::wstring serialNumber = L"N/A";
    std::wstring uuid = L"N/A";
    std::wstring biosVendor = L"N/A";
    std::wstring biosVersion = L"N/A";
    std::wstring biosReleaseDate = L"N/A";
    std::wstring firmwareType = L"Unknown";
};

struct MemoryStatusInfo {
    ULONGLONG totalPhysBytes{0};
    ULONGLONG availPhysBytes{0};
    ULONGLONG pageFileLimitBytes{0};
    double loadPercentage{0.0};
};

struct TpmInfo {
    bool present{false};
    bool ready{false};
    std::wstring specVersion = L"N/A";
    std::wstring manufacturer = L"N/A";
    std::wstring manufacturerVersion = L"N/A";
};

class MachinfoOptions {
public:
    bool verbose{false};
    bool quiet{false};
    bool tpmWmi{false};
    bool showBios{false};
    bool showCpu{false};
    bool showMemory{false};
    bool showTpm{false};
    bool showAll{false};
    enum class OutputFormat { Table, Csv, Json } format{OutputFormat::Table};
    std::vector<std::wstring> filters;

    static void printHelp(const wchar_t* /*progName*/ = L"machinfo") {
        std::wcout << LR"(machinfo(1)             CrossShell for UNIX Reference Manual                 machinfo(1)

    NAME
        machinfo - display machine firmware, CPU topology, memory, TPM, and OS details

    SYNOPSIS
        machinfo [OPTION]...

    DESCRIPTION
        Displays comprehensive system and hardware information including firmware version,
        CPU topology and capabilities, physical memory, Trusted Platform Module (TPM) status,
        and operating system details. This implementation accepts the HP-UX-style qualifiers
        commonly used for machine inventory and diagnostics on legacy systems.

    OPTIONS
        -a, --all
            Show the complete machine summary (default behavior).

        -b, --bios
            Display BIOS and system firmware details.

        -c, --cpu
            Display CPU topology and processor-related data.

        -m, --memory
            Display memory and paging summary information.

        -t, --tpm
            Display TPM status and version details.

        -v, --verbose, --extended, -x
            Show extended processor capability details including cache levels and
            virtualization features.

        -q, --quiet, --brief, -s, --summary
            Show condensed summary output (host, CPU, memory, firmware, TPM, OS).

        -f, --format, --output, -o <format>
            Select output format explicitly: table, csv, or json.

        --tpm-wmi
            Enable TPM WMI enrichment (disabled by default for stability).

        --table
            Display aligned summary output (default).

        --csv
            Display CSV-formatted output.

        --json
            Display JSON-formatted output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        machinfo
            Display full system information.

        machinfo -q
            Display condensed system summary.

        machinfo -v
            Show extended processor details.

        machinfo --bios
            Display BIOS and firmware details.

        machinfo --cpu --format csv
            Display processor information in CSV form.

        machinfo --json
            Output system information in JSON format.

    CrossShell for UNIX                                                   machinfo(1)
)";
    }

    static bool parse(int argc, wchar_t* argv[], MachinfoOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-h" || arg == L"--h" || arg == L"--help" || arg == L"/?" || arg == L"-?" || arg == L"--?" || arg == L"/help") {
                printHelp(argv[0]);
                std::exit(0);
            } else if (arg == L"-V" || arg == L"--version") {
                std::wcout << L"machinfo 1.0.0\n";
                std::exit(0);
            } else if (arg == L"-v" || arg == L"--verbose" || arg == L"--extended" || arg == L"-x") {
                opts.verbose = true;
            } else if (arg == L"-q" || arg == L"--quiet" || arg == L"--brief" || arg == L"-s" || arg == L"--summary") {
                opts.quiet = true;
            } else if (arg == L"-a" || arg == L"--all") {
                opts.showAll = true;
                opts.quiet = false;
            } else if (arg == L"-b" || arg == L"--bios") {
                opts.showBios = true;
            } else if (arg == L"-c" || arg == L"--cpu") {
                opts.showCpu = true;
            } else if (arg == L"-m" || arg == L"--memory") {
                opts.showMemory = true;
            } else if (arg == L"-t" || arg == L"--tpm") {
                opts.showTpm = true;
            } else if (arg == L"-f" || arg == L"--format" || arg == L"-o" || arg == L"--output") {
                if (i + 1 < argc) {
                    std::wstring next = argv[++i];
                    if (next == L"csv") opts.format = OutputFormat::Csv;
                    else if (next == L"json") opts.format = OutputFormat::Json;
                    else opts.format = OutputFormat::Table;
                }
            } else if (arg == L"--tpm-wmi") {
                opts.tpmWmi = true;
            } else if (arg == L"--table") {
                opts.format = OutputFormat::Table;
            } else if (arg == L"--csv") {
                opts.format = OutputFormat::Csv;
            } else if (arg == L"--json") {
                opts.format = OutputFormat::Json;
            } else if (arg == L"-") {
                std::wstring filter;
                while (std::wcin >> filter) opts.filters.push_back(filter);
            } else if (!arg.empty() && arg[0] != L'-') {
                opts.filters.push_back(arg);
            } else {
                std::wcerr << L"Unknown option: " << arg << L"\nUse -h for help.\n";
                return false;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. SMBIOS & REGISTRY PARSERS
// ============================================================================

class SmbiosParser {
public:
    static std::wstring getSmbiosString(const BYTE* headerPtr, BYTE stringIndex, const BYTE* endPtr) {
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

    static SystemFirmwareInfo parseFirmware() {
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
};

class RegistryHelper {
public:
    static bool readString(HKEY hRoot, const wchar_t* subkeyPath, const wchar_t* valueName, std::wstring& outValue) {
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

    static bool readDword(HKEY hRoot, const wchar_t* subkeyPath, const wchar_t* valueName, DWORD& outValue) {
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
};

// ============================================================================
// 3. WMI & TPM DIAGNOSTICS PROVIDER
// ============================================================================

class TpmDiagnosticsProvider {
private:
    static std::wstring manufacturerIdToString(DWORD id) {
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

public:
    static TpmInfo query(bool enableTpmWmi) {
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
};

// ============================================================================
// 4. HARDWARE SCANNER ENGINE
// ============================================================================

class HardwareDiagnosticsScanner {
private:
    static bool checkAVX2Support() {
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

public:
    static CpuTopology queryCpuTopology() {
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

    static MemoryStatusInfo queryMemoryStatus() {
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

    static std::wstring queryWindowsVersion() {
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
};

// ============================================================================
// 5. STRUCTURED OUTPUT REPORTER
// ============================================================================

class MachinfoReporter {
public:
    static std::wstring formatMemorySize(ULONGLONG bytes) {
        double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
        double gb = mb / 1024.0;
        wchar_t buf[64];
        if (gb >= 1.0) {
            swprintf_s(buf, 64, L"%.0f MB (%.2f GB)", mb, gb);
        } else {
            swprintf_s(buf, 64, L"%.0f MB", mb);
        }
        return std::wstring(buf);
    }

    static std::wstring jsonEscape(const std::wstring& value) {
        std::wstring out;
        for (wchar_t c : value) {
            if (c == L'"' || c == L'\\') out += L'\\';
            if (c == L'\n') out += L"\\n";
            else out += c;
        }
        return out;
    }

    static void report(const CpuTopology& cpu, const SystemFirmwareInfo& fw, const MemoryStatusInfo& mem,
                       const TpmInfo& tpm, const std::wstring& osVersion, const MachinfoOptions& opts) {
        const bool hasSpecificSection = opts.showBios || opts.showCpu || opts.showMemory || opts.showTpm;
        const bool displayAll = opts.showAll || !hasSpecificSection;
        const bool displayBios = displayAll || opts.showBios;
        const bool displayCpu = displayAll || opts.showCpu;
        const bool displayMemory = displayAll || opts.showMemory;
        const bool displayTpm = displayAll || opts.showTpm;

        if (opts.format == MachinfoOptions::OutputFormat::Json) {
            std::wcout << L"{\n";
            std::vector<std::wstring> fields;
            if (displayAll || displayBios) {
                fields.push_back(L"  \"manufacturer\":\"" + jsonEscape(fw.manufacturer) + L"\"");
                fields.push_back(L"  \"product\":\"" + jsonEscape(fw.productName) + L"\"");
                fields.push_back(L"  \"serial\":\"" + jsonEscape(fw.serialNumber) + L"\"");
                if (!displayAll) {
                    fields.push_back(L"  \"firmwareType\":\"" + jsonEscape(fw.firmwareType) + L"\"");
                    fields.push_back(L"  \"biosVendor\":\"" + jsonEscape(fw.biosVendor) + L"\"");
                    fields.push_back(L"  \"biosVersion\":\"" + jsonEscape(fw.biosVersion) + L"\"");
                }
            }
            if (displayAll || displayCpu) {
                fields.push_back(L"  \"cpu\":\"" + jsonEscape(cpu.name) + L"\"");
            }
            if (displayAll || displayMemory) {
                fields.push_back(L"  \"memory\":\"" + jsonEscape(formatMemorySize(mem.totalPhysBytes)) + L"\"");
            }
            if (displayAll) {
                fields.push_back(L"  \"os\":\"" + jsonEscape(osVersion) + L"\"");
            }
            if (displayAll || displayTpm) {
                fields.push_back(L"  \"tpmPresent\":" + std::wstring(tpm.present ? L"true" : L"false"));
                if (!displayAll && tpm.specVersion != L"N/A") {
                    fields.push_back(L"  \"tpmSpecVersion\":\"" + jsonEscape(tpm.specVersion) + L"\"");
                }
            }
            for (size_t i = 0; i < fields.size(); ++i) {
                std::wcout << fields[i] << (i + 1 < fields.size() ? L",\n" : L"\n");
            }
            std::wcout << L"}\n";
            return;
        }

        if (opts.format == MachinfoOptions::OutputFormat::Csv) {
            std::wcout << L"Field,Value\n";
            if (displayAll || displayBios) {
                std::wcout << L"Manufacturer,\"" << fw.manufacturer << L"\"\n"
                          << L"Product,\"" << fw.productName << L"\"\n";
                if (!displayAll) {
                    std::wcout << L"BIOS Vendor,\"" << fw.biosVendor << L"\"\n"
                              << L"BIOS Version,\"" << fw.biosVersion << L"\"\n";
                }
            }
            if (displayAll || displayCpu) {
                std::wcout << L"CPU,\"" << cpu.name << L"\"\n";
            }
            if (displayAll || displayMemory) {
                std::wcout << L"Memory,\"" << formatMemorySize(mem.totalPhysBytes) << L"\"\n";
            }
            if (displayAll) {
                std::wcout << L"OS,\"" << osVersion << L"\"\n";
            }
            if (displayAll || displayTpm) {
                std::wcout << L"TPM Present,\"" << (tpm.present ? L"Yes" : L"No") << L"\"\n";
            }
            return;
        }

        if (opts.quiet) {
            if (displayAll) {
                std::wcout << L"Host:     " << fw.productName << L" (" << fw.manufacturer << L")\n";
            }
            if (displayCpu) {
                std::wcout << L"CPU:      " << cpu.name << L" (" << cpu.coreCount << L" Cores, " << cpu.logicalCount << L" Threads)\n";
            }
            if (displayMemory) {
                std::wcout << L"Memory:   " << formatMemorySize(mem.totalPhysBytes) << L"\n";
            }
            if (displayBios) {
                std::wcout << L"Firmware: " << fw.firmwareType << L" " << fw.biosVersion << L"\n";
            }
            if (displayTpm) {
                std::wcout << L"TPM:      " << (tpm.present ? (tpm.specVersion == L"N/A" ? L"Present" : (L"Present (" + tpm.specVersion + L")")) : L"Not Detected") << L"\n";
            }
            if (displayAll) {
                std::wcout << L"OS:       " << osVersion << L"\n";
            }
            return;
        }

        std::wcout << L"\nSystem and Hardware Summary v10.9.0\n\n";

        if (displayAll) {
            std::wcout << L"Machine Info:\n"
                       << L"    Manufacturer:       " << fw.manufacturer << L"\n"
                       << L"    Product Name:       " << fw.productName << L"\n"
                       << L"    Serial Number:      " << fw.serialNumber << L"\n"
                       << L"    UUID:               " << fw.uuid << L"\n"
                       << L"    Architecture:       64-bit x86-64 Architecture\n\n";
        }

        if (displayBios) {
            std::wcout << L"Firmware / BIOS Info:\n"
                       << L"    Firmware Type:      " << fw.firmwareType << L"\n"
                       << L"    BIOS Vendor:        " << fw.biosVendor << L"\n"
                       << L"    BIOS Version:       " << fw.biosVersion << L"\n"
                       << L"    Release Date:       " << fw.biosReleaseDate << L"\n\n";
        }

        if (displayTpm) {
            std::wcout << L"TPM / Security Info:\n"
                       << L"    TPM Present:        " << (tpm.present ? L"Yes" : L"No") << L"\n"
                       << L"    TPM Ready:          " << (tpm.ready ? L"Yes" : L"No") << L"\n"
                       << L"    TPM Spec Version:   " << tpm.specVersion << L"\n"
                       << L"    Manufacturer:       " << tpm.manufacturer << L"\n"
                       << L"    Manufacturer Ver:   " << tpm.manufacturerVersion << L"\n\n";
        }

        if (displayCpu) {
            std::wcout << L"Processor Info:\n"
                       << L"    Model:              " << cpu.name << L"\n"
                       << L"    Clock Speed:        " << cpu.clockMhz << L" MHz\n"
                       << L"    Sockets:            " << (cpu.socketCount > 0 ? cpu.socketCount : 1) << L"\n"
                       << L"    Physical Cores:     " << cpu.coreCount << L"\n"
                       << L"    Logical Processors: " << cpu.logicalCount << L"\n"
                       << L"    NUMA Nodes:         " << (cpu.numaNodeCount > 0 ? cpu.numaNodeCount : 1) << L"\n";

            if (opts.verbose) {
                std::wcout << L"    L1 Cache:           " << formatMemorySize(cpu.l1CacheBytes) << L"\n"
                           << L"    L2 Cache:           " << formatMemorySize(cpu.l2CacheBytes) << L"\n"
                           << L"    L3 Cache:           " << formatMemorySize(cpu.l3CacheBytes) << L"\n"
                           << L"    Hardware VT:        " << (cpu.supportsVirtualization ? L"Enabled" : L"Disabled / Unavailable") << L"\n";
            }
            std::wcout << L"\n";
        }

        if (displayMemory) {
            std::wcout << L"Memory Info:\n"
                       << L"    Total Physical:     " << formatMemorySize(mem.totalPhysBytes) << L"\n"
                       << L"    Available Physical: " << formatMemorySize(mem.availPhysBytes) << L"\n"
                       << L"    Memory Utilization: " << mem.loadPercentage << L"%\n"
                       << L"    Commit Limit:       " << formatMemorySize(mem.pageFileLimitBytes) << L"\n\n";
        }

        if (displayAll) {
            std::wcout << L"Operating System Info:\n"
                       << L"    OS Name:            " << osVersion << L"\n";
        }
    }
};

// ============================================================================
// 6. APPLICATION CONTROLLER
// ============================================================================

class MachinfoEngine {
private:
    MachinfoOptions options;

public:
    explicit MachinfoEngine(MachinfoOptions opts) : options(std::move(opts)) {}

    int execute() {
        const bool enableTpmWmi = options.tpmWmi || (GetEnvironmentVariableW(L"MACHINFO_ENABLE_TPM_WMI", nullptr, 0) > 0);

        CpuTopology cpu = HardwareDiagnosticsScanner::queryCpuTopology();
        SystemFirmwareInfo fw = SmbiosParser::parseFirmware();
        MemoryStatusInfo mem = HardwareDiagnosticsScanner::queryMemoryStatus();
        TpmInfo tpm = TpmDiagnosticsProvider::query(enableTpmWmi);
        std::wstring osVersion = HardwareDiagnosticsScanner::queryWindowsVersion();

        MachinfoReporter::report(cpu, fw, mem, tpm, osVersion, options);
        return 0;
    }
};

class MachinfoApp {
public:
    static int run(int argc, wchar_t* argv[]) {
        _setmode(_fileno(stdout), _O_U16TEXT);
        _setmode(_fileno(stderr), _O_U16TEXT);

        MachinfoOptions options;
        if (!MachinfoOptions::parse(argc, argv, options)) {
            return 1;
        }
        MachinfoEngine engine(std::move(options));
        return engine.execute();
    }
};

int wmain(int argc, wchar_t* argv[]) {
    return MachinfoApp::run(argc, argv);
}