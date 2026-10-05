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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <regstr.h>
#include <cfgmgr32.h>
#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <unordered_map>
#include <cwctype>
#include <cstdio>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// ============================================================================
// 1. CONSOLE CONFIGURATION & RAII HANDLES
// ============================================================================

class ConsoleManager {
public:
    static void ConfigureNativePipes() {
        HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD consoleMode = 0;
        const bool stdoutConsole = GetFileType(stdoutHandle) == FILE_TYPE_CHAR && GetConsoleMode(stdoutHandle, &consoleMode);
        _setmode(_fileno(stdout), stdoutConsole ? _O_U16TEXT : _O_U8TEXT);
        _setmode(_fileno(stderr), _O_U8TEXT);
    }
};

class ScopedDevInfo {
private:
    HDEVINFO m_handle = INVALID_HANDLE_VALUE;

public:
    explicit ScopedDevInfo(HDEVINFO handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}
    ~ScopedDevInfo() {
        if (isValid()) {
            SetupDiDestroyDeviceInfoList(m_handle);
        }
    }

    bool isValid() const {
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
    }

    HDEVINFO get() const { return m_handle; }

    ScopedDevInfo(const ScopedDevInfo&) = delete;
    ScopedDevInfo& operator=(const ScopedDevInfo&) = delete;

    ScopedDevInfo(ScopedDevInfo&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    ScopedDevInfo& operator=(ScopedDevInfo&& other) noexcept {
        if (this != &other) {
            if (isValid()) SetupDiDestroyDeviceInfoList(m_handle);
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
};

// ============================================================================
// 2. DATA MODELS & CONFIGURATION
// ============================================================================

struct DeviceRecord {
    unsigned int index = 0;
    std::wstring className;
    std::wstring hwPath;
    std::wstring instanceId;
    std::wstring hardwareId;
    std::wstring driver;
    std::wstring swState;
    std::wstring hwType;
    std::wstring description;
    std::wstring manufacturer;
    std::wstring location;
    std::wstring health;
    bool present = false;
};

struct IoscanOptions {
    bool fullView = false;
    bool summaryView = false;
    bool agileView = false;
    bool compactView = false;
    bool listDeviceFiles = false;
    bool showAlias = false;
    bool showDevicePath = false;
    bool processorThreads = false;
    bool kernelOnly = false;
    bool presentOnly = true;
    bool usableOnly = false;
    bool localOnly = false;
    bool staleOnly = false;
    bool unclaimedInterfacesOnly = false;
    bool showScanTime = false;
    bool listPendingBindings = false;
    std::wstring classFilter;
    std::wstring driverFilter;
    std::wstring hwPathFilter;
    int instanceFilter = -1;
    std::wstring mappingKeyword;
    std::wstring propertyName;
    int outputFormat = 0; // 0=default/table, 1=json, 2=csv, 3=table
    std::wstring pipeCommand;
};

// ============================================================================
// 3. DEVICE PROPERTY RESOLVER & MAPPER
// ============================================================================

class DeviceMapper {
public:
    static std::wstring ToUpper(std::wstring str) {
        std::transform(str.begin(), str.end(), str.begin(), ::towupper);
        return str;
    }

    static std::wstring MapHpUxClassToWindows(const std::wstring& hpuxClass) {
        std::wstring upperClass = ToUpper(hpuxClass);
        static const std::unordered_map<std::wstring, std::wstring> classMap = {
            { L"DISK",      L"DiskDrive" },
            { L"LAN",       L"Net" },
            { L"NET",       L"Net" },
            { L"DISPLAY",   L"Display" },
            { L"GRAPHICS",  L"Display" },
            { L"TTY",       L"Ports" },
            { L"PROCESSOR", L"Processor" },
            { L"CPU",       L"Processor" },
            { L"TAPE",      L"TapeDrive" },
            { L"SCSI",      L"SCSIAdapter" },
            { L"FC",        L"SCSIAdapter" },
            { L"USB",       L"USB" },
            { L"SOUND",     L"MEDIA" },
            { L"SYSTEM",    L"System" }
        };

        auto it = classMap.find(upperClass);
        return (it != classMap.end()) ? it->second : hpuxClass;
    }

    static std::wstring GetDeviceProperty(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDevInfoData, DWORD property) {
        DWORD dataType = 0;
        DWORD reqSize = 0;

        SetupDiGetDeviceRegistryPropertyW(hDevInfo, pDevInfoData, property, &dataType, nullptr, 0, &reqSize);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || reqSize == 0) {
            return L"";
        }

        std::vector<wchar_t> buffer((reqSize / sizeof(wchar_t)) + 1, L'\0');
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, pDevInfoData, property, &dataType, 
                                              reinterpret_cast<PBYTE>(buffer.data()), reqSize, nullptr)) {
            if (dataType == REG_SZ || dataType == REG_EXPAND_SZ || dataType == REG_MULTI_SZ) {
                return std::wstring(buffer.data());
            }
        }
        return L"";
    }

    static std::wstring GetDeviceHardwarePath(HDEVINFO hDevInfo, PSP_DEVINFO_DATA pDevInfoData) {
        std::wstring location = GetDeviceProperty(hDevInfo, pDevInfoData, SPDRP_LOCATION_INFORMATION);
        if (!location.empty()) {
            return location;
        }

        wchar_t devId[MAX_DEVICE_ID_LEN] = { 0 };
        if (CM_Get_Device_IDW(pDevInfoData->DevInst, devId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
            return std::wstring(devId);
        }
        return L"UNKNOWN";
    }

    static std::wstring GetDeviceInstanceId(PSP_DEVINFO_DATA pDevInfoData) {
        wchar_t devId[MAX_DEVICE_ID_LEN] = { 0 };
        if (CM_Get_Device_IDW(pDevInfoData->DevInst, devId, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS) {
            return std::wstring(devId);
        }
        return L"UNKNOWN";
    }

    static bool IsPresent(PSP_DEVINFO_DATA pDevInfoData) {
        ULONG status = 0;
        ULONG problemNumber = 0;
        return CM_Get_DevNode_Status(&status, &problemNumber, pDevInfoData->DevInst, 0) == CR_SUCCESS &&
               problemNumber != CM_PROB_PHANTOM;
    }

    static bool EqualsInsensitive(const std::wstring& left, const std::wstring& right) {
        return ToUpper(left) == ToUpper(right);
    }

    static bool StartsWithInsensitive(const std::wstring& value, const std::wstring& prefix) {
        if (prefix.size() > value.size()) return false;
        return ToUpper(value.substr(0, prefix.size())) == ToUpper(prefix);
    }

    static std::wstring DetermineSwState(PSP_DEVINFO_DATA pDevInfoData) {
        ULONG status = 0, problemNumber = 0;
        if (CM_Get_DevNode_Status(&status, &problemNumber, pDevInfoData->DevInst, 0) == CR_SUCCESS) {
            if (status & DN_HAS_PROBLEM) {
                if (problemNumber == CM_PROB_DISABLED) return L"DISABLED";
                if (problemNumber == CM_PROB_FAILED_START) return L"FAILED";
                return L"ERROR";
            }
            if (status & DN_STARTED) {
                return L"CLAIMED";
            }
            if (!(status & DN_DRIVER_LOADED)) {
                return L"UNCONFIGURED";
            }
            return L"NO_HW";
        }
        return L"UNCONFIGURED";
    }

    static std::wstring DetermineHwType(const std::wstring& className) {
        std::wstring wClass = ToUpper(className);
        if (wClass == L"PROCESSOR") return L"PROCESSOR";
        if (wClass == L"NET") return L"INTERFACE";
        if (wClass == L"SYSTEM" || wClass == L"SCSIADAPTER") return L"BUS";
        return L"DEVICE";
    }
};

// ============================================================================
// 4. DEVICE ENUMERATOR
// ============================================================================

class DeviceEnumerator {
public:
    static bool Enumerate(const IoscanOptions& opts, std::vector<DeviceRecord>& devices, std::unordered_map<std::wstring, int>& summaryCount) {
        DWORD flags = DIGCF_ALLCLASSES;
        if (opts.presentOnly && !opts.kernelOnly && !opts.staleOnly) {
            flags |= DIGCF_PRESENT;
        }

        ScopedDevInfo hDevInfo(SetupDiGetClassDevsW(nullptr, nullptr, nullptr, flags));
        if (!hDevInfo.isValid()) {
            std::wcerr << L"Failed to query system device information. Error: " << GetLastError() << L"\n";
            return false;
        }

        SP_DEVINFO_DATA devInfoData = { 0 };
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        devices.clear();
        summaryCount.clear();
        std::unordered_map<std::wstring, unsigned int> classInstances;

        for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo.get(), i, &devInfoData); ++i) {
            std::wstring className = DeviceMapper::GetDeviceProperty(hDevInfo.get(), &devInfoData, SPDRP_CLASS);
            if (className.empty()) className = L"Unknown";

            const unsigned int instance = classInstances[DeviceMapper::ToUpper(className)]++;

            if (!opts.classFilter.empty()) {
                if (!DeviceMapper::EqualsInsensitive(className, opts.classFilter)) {
                    continue;
                }
            }

            std::wstring swState = DeviceMapper::DetermineSwState(&devInfoData);

            std::wstring desc = DeviceMapper::GetDeviceProperty(hDevInfo.get(), &devInfoData, SPDRP_FRIENDLYNAME);
            if (desc.empty()) {
                desc = DeviceMapper::GetDeviceProperty(hDevInfo.get(), &devInfoData, SPDRP_DEVICEDESC);
            }
            if (desc.empty()) desc = L"N/A";

            std::wstring driver = DeviceMapper::GetDeviceProperty(hDevInfo.get(), &devInfoData, SPDRP_SERVICE);
            if (driver.empty()) driver = L"NO_DRIVER";

            std::wstring hwPath = DeviceMapper::GetDeviceHardwarePath(hDevInfo.get(), &devInfoData);
            std::wstring instanceId = DeviceMapper::GetDeviceInstanceId(&devInfoData);
            std::wstring hardwareId = DeviceMapper::GetDeviceProperty(hDevInfo.get(), &devInfoData, SPDRP_HARDWAREID);
            std::wstring manufacturer = DeviceMapper::GetDeviceProperty(hDevInfo.get(), &devInfoData, SPDRP_MFG);
            std::wstring location = DeviceMapper::GetDeviceProperty(hDevInfo.get(), &devInfoData, SPDRP_LOCATION_INFORMATION);
            const bool present = DeviceMapper::IsPresent(&devInfoData);

            if (!opts.driverFilter.empty() && !DeviceMapper::EqualsInsensitive(driver, opts.driverFilter)) continue;
            if (!opts.hwPathFilter.empty() &&
                !DeviceMapper::StartsWithInsensitive(hwPath, opts.hwPathFilter) &&
                !DeviceMapper::StartsWithInsensitive(instanceId, opts.hwPathFilter)) continue;
            if (opts.instanceFilter >= 0 && instance != static_cast<unsigned int>(opts.instanceFilter)) continue;
            if (opts.usableOnly && (driver == L"NO_DRIVER" || swState != L"CLAIMED")) continue;
            if (opts.staleOnly && present) continue;

            DeviceRecord rec;
            rec.index = instance;
            rec.className = className;
            rec.hwPath = hwPath;
            rec.instanceId = instanceId;
            rec.hardwareId = hardwareId.empty() ? L"N/A" : hardwareId;
            rec.driver = driver;
            rec.swState = swState;
            rec.hwType = DeviceMapper::DetermineHwType(className);
            rec.description = desc;
            rec.manufacturer = manufacturer.empty() ? L"N/A" : manufacturer;
            rec.location = location.empty() ? L"N/A" : location;
            rec.health = swState == L"CLAIMED" ? L"ONLINE" :
                         (swState == L"DISABLED" ? L"OFFLINE" : L"UNUSABLE");
            rec.present = present;

            if (opts.unclaimedInterfacesOnly &&
                (rec.hwType != L"INTERFACE" || rec.swState == L"CLAIMED")) continue;

            devices.push_back(rec);
            summaryCount[className]++;
        }
        return true;
    }
};

// ============================================================================
// 5. OUTPUT FORMATTER
// ============================================================================

class OutputFormatter {
public:
    static void PrintHelp(const wchar_t* progName) {
            (void)progName;
           std::wcout << LR"HELP(ioscan(1)                CrossShell for UNIX Reference Manual                  ioscan(1)

    NAME
        ioscan - enumerate Windows hardware devices

    SYNOPSIS
        ioscan [OPTIONS]

    DESCRIPTION
        Enumerates hardware through Windows SetupAPI and Configuration Manager.
        The default view scans present hardware and renders a device table.

    OPTIONS
        -A                 Show alias paths; requires -N.
        -a                 Show processor socket, core, and thread fields.
        -B                 List pending deferred bindings (none on Windows).
        -C CLASS           Restrict output to a device class or alias.
        -D                 Defer driver binding (unsupported on Windows).
        -d DRIVER          Restrict output to devices controlled by DRIVER.
        -e                 Include the Windows device-instance path.
        -f                 Generate a full listing.
        -F                 Generate a compact colon-delimited listing.
        -H HW_PATH         Restrict output to a hardware-path subtree.
        -I INSTANCE        Restrict output to a class instance number.
        -k                 Read the full kernel device tree, including phantom nodes.
        -l                 Restrict output to locally connected devices.
        -m KEYWORD         Show lun, dsf, or hw_path mappings.
        -M DRIVER          Bind DRIVER (unsupported on Windows).
        -n                 List Windows device-instance identifiers.
        -N                 Select agile view mode.
        -P PROPERTY        Display an agile-view property; requires -N.
        -R                 Remove a deferred binding (unsupported on Windows).
        -s                 List stale (non-present) device nodes.
        -t                 Display the last scan time; cannot be combined.
        -u                 List usable devices with loaded drivers.
        -U                 List unclaimed INTERFACE nodes.
        --summary          Display summary counts by device class.
        --json             Emit JSON device records.
        --csv              Emit CSV device records.
        --table            Emit tabular device records.
        --pipe COMMAND     Send formatted records through COMMAND.
        -h, -?, /?, --help Display this comprehensive reference manual and exit.

    EXAMPLES
        ioscan -fnC disk
        ioscan -C lan
        ioscan -u

    EXIT STATUS
        0          Help or successful rendering.
        1          Invalid option, enumeration, or pipe failure.

    CrossShell for UNIX                                                       ioscan(1)
    )HELP";
    }

    static std::wstring PropertyValue(const DeviceRecord& dev, const std::wstring& property) {
        const std::wstring name = DeviceMapper::ToUpper(property);
        if (name == L"CLASS") return dev.className;
        if (name == L"DRIVER" || name == L"MODULE_NAME") return dev.driver;
        if (name == L"HW_PATH" || name == L"ALIAS_PATH") return dev.hwPath;
        if (name == L"ID_BYTES" || name == L"UNIQ_NAME" || name == L"WWID") return dev.hardwareId;
        if (name == L"INSTANCE") return std::to_wstring(dev.index);
        if (name == L"SW_STATE") return dev.swState;
        if (name == L"HW_TYPE") return dev.hwType;
        if (name == L"DESCRIPTION") return dev.description;
        if (name == L"HEALTH") return dev.health;
        if (name == L"PHYSICAL_LOCATION") return dev.location;
        if (name == L"IS_BLOCK") return DeviceMapper::EqualsInsensitive(dev.className, L"DiskDrive") ? L"1" : L"0";
        if (name == L"IS_CHAR") return L"1";
        if (name == L"IS_PSEUDO" || name == L"IS_REMOTE") return L"0";
        if (name == L"BUS_TYPE") return dev.hwType == L"BUS" ? dev.className : L"N/A";
        if (name == L"CDIO" || name == L"B_MAJOR" || name == L"C_MAJOR" || name == L"MINOR" ||
            name == L"MODULE_PATH" || name == L"CARD_INSTANCE" || name == L"ERROR_RECOVERY" ||
            name == L"IS_INST_REPLACEABLE" || name == L"MS_SCAN_TIME") return L"N/A";
        return L"";
    }

    static bool IsKnownProperty(const std::wstring& property) {
        static const std::vector<std::wstring> names = {
            L"BUS_TYPE", L"CDIO", L"IS_BLOCK", L"IS_CHAR", L"IS_PSEUDO", L"B_MAJOR", L"C_MAJOR",
            L"MINOR", L"CLASS", L"DRIVER", L"HW_PATH", L"ID_BYTES", L"INSTANCE", L"MODULE_PATH",
            L"MODULE_NAME", L"SW_STATE", L"HW_TYPE", L"DESCRIPTION", L"CARD_INSTANCE", L"IS_REMOTE",
            L"HEALTH", L"ERROR_RECOVERY", L"IS_INST_REPLACEABLE", L"WWID", L"UNIQ_NAME", L"ALIAS_PATH",
            L"PHYSICAL_LOCATION", L"MS_SCAN_TIME"
        };
        const std::wstring upper = DeviceMapper::ToUpper(property);
        return std::find(names.begin(), names.end(), upper) != names.end();
    }

    static int RenderOutput(const IoscanOptions& opts, const std::vector<DeviceRecord>& devices, const std::unordered_map<std::wstring, int>& summaryCount) {
        if (opts.showScanTime) {
            const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm localTime = {};
            localtime_s(&localTime, &now);
            std::wcout << std::put_time(&localTime, L"%Y-%m-%d %H:%M:%S") << L"\n";
            return 0;
        }

        if (opts.listPendingBindings) {
            return 0;
        }

        if (!opts.propertyName.empty()) {
            if (!IsKnownProperty(opts.propertyName)) {
                std::wcerr << L"ioscan: unknown property: " << opts.propertyName << L"\n";
                return 1;
            }
            for (const auto& dev : devices) {
                std::wcout << dev.hwPath << L":" << opts.propertyName << L":"
                           << PropertyValue(dev, opts.propertyName) << L"\n";
            }
            return 0;
        }

        if (!opts.mappingKeyword.empty()) {
            const std::wstring keyword = DeviceMapper::ToUpper(opts.mappingKeyword);
            if (keyword != L"LUN" && keyword != L"DSF" && keyword != L"HW_PATH") {
                std::wcerr << L"ioscan: unsupported mapping keyword: " << opts.mappingKeyword
                           << L" (expected lun, dsf, or hw_path)\n";
                return 1;
            }
            for (const auto& dev : devices) {
                std::wcout << dev.hwPath << L":" << dev.instanceId << L"\n";
            }
            return 0;
        }

        if (opts.processorThreads) {
            for (const auto& dev : devices) {
                if (DeviceMapper::EqualsInsensitive(dev.className, L"Processor")) {
                    std::wcout << dev.index << L":" << dev.index << L":0\n";
                }
            }
            return 0;
        }

        if (opts.compactView) {
            for (const auto& dev : devices) {
                std::wcout << L":" << L":" << L"0:1:0:-1:-1:0:"
                           << dev.className << L":" << dev.driver << L":" << dev.hwPath << L":"
                           << dev.hardwareId << L":" << dev.index << L"::" << dev.driver << L":"
                           << dev.swState << L":" << dev.hwType << L":" << dev.description;
                if (opts.showAlias) std::wcout << L":" << dev.hwPath;
                if (opts.showDevicePath) std::wcout << L":" << dev.instanceId;
                std::wcout << L"\n";
                if (opts.listDeviceFiles) std::wcout << dev.instanceId << L"\n";
            }
            return 0;
        }
        if (opts.outputFormat != 0 || !opts.pipeCommand.empty()) {
            std::wstringstream output;
            if (opts.outputFormat == 1) {
                output << L"[";
                for (size_t i = 0; i < devices.size(); ++i) {
                    if (i) output << L",";
                    const auto& dev = devices[i];
                    output << L"{\"class\":\"" << dev.className << L"\",\"index\":" << dev.index 
                           << L",\"path\":\"" << dev.hwPath << L"\",\"driver\":\"" << dev.driver 
                           << L"\",\"state\":\"" << dev.swState << L"\",\"type\":\"" << dev.hwType 
                           << L"\",\"description\":\"" << dev.description << L"\"}";
                }
                output << L"]\n";
            } else if (opts.outputFormat == 2) {
                output << L"class,index,path,driver,state,type,description\n";
                for (const auto& dev : devices) {
                    output << dev.className << L"," << dev.index << L"," << dev.hwPath << L"," 
                           << dev.driver << L"," << dev.swState << L"," << dev.hwType << L"," << dev.description << L"\n";
                }
            } else {
                output << L"CLASS\tINDEX\tPATH\tDRIVER\tSTATE\tTYPE\tDESCRIPTION\n";
                for (const auto& dev : devices) {
                    output << dev.className << L"\t" << dev.index << L"\t" << dev.hwPath << L"\t" 
                           << dev.driver << L"\t" << dev.swState << L"\t" << dev.hwType << L"\t" << dev.description << L"\n";
                }
            }

            std::wstring text = output.str();
            if (!opts.pipeCommand.empty()) {
                FILE* pipe = _wpopen(opts.pipeCommand.c_str(), L"w");
                if (!pipe) return 1;
                int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
                std::string narrow(size, '\0');
                WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
                fwrite(narrow.data(), 1, narrow.size(), pipe);
                _pclose(pipe);
            } else {
                std::wcout << text;
            }
            return 0;
        }

        if (opts.summaryView) {
            std::wcout << L"====================================================\n";
            std::wcout << std::left << std::setw(25) << L"Class" << std::setw(15) << L"Device Count" << L"\n";
            std::wcout << L"====================================================\n";
            for (const auto& kv : summaryCount) {
                std::wcout << std::left << std::setw(25) << kv.first << std::setw(15) << kv.second << L"\n";
            }
            std::wcout << L"====================================================\n";
            std::wcout << L"Total Hardware Devices: " << devices.size() << L"\n";
            return 0;
        }

        std::wcout << std::left 
                   << std::setw(14) << L"Class"
                   << std::setw(6)  << L"I"
                   << std::setw(opts.fullView ? 42 : 28) << L"H/W Path"
                   << std::setw(16) << L"Driver"
                   << std::setw(14) << L"S/W State"
                   << std::setw(12) << L"H/W Type"
                   << L"Description\n";

        std::wcout << std::wstring(opts.fullView ? 125 : 105, L'=') << L"\n";

        for (const auto& dev : devices) {
            std::wstring pathDisplay = opts.agileView ? dev.instanceId : dev.hwPath;
            if (!opts.fullView && pathDisplay.length() > 26) {
                pathDisplay = L"..." + pathDisplay.substr(pathDisplay.length() - 23);
            }

            std::wcout << std::left
                       << std::setw(14) << dev.className.substr(0, 13)
                       << std::setw(6)  << dev.index
                       << std::setw(opts.fullView ? 42 : 28) << pathDisplay
                       << std::setw(16) << dev.driver.substr(0, 15)
                       << std::setw(14) << dev.swState
                       << std::setw(12) << dev.hwType
                       << dev.description << L"\n";
            if (opts.showAlias) {
                std::wcout << L"  Alias path: " << dev.hwPath << L"\n";
            }
            if (opts.showDevicePath) {
                std::wcout << L"  Windows device path: " << dev.instanceId << L"\n";
            }
            if (opts.listDeviceFiles) {
                std::wcout << L"  " << dev.instanceId << L"\n";
            }
        }

        return 0;
    }
};

// ============================================================================
// 6. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    static bool TakeValue(const std::wstring& arg, size_t& position, int argc, wchar_t* argv[], int& index,
                          const wchar_t option, std::wstring& value) {
        if (position + 1 < arg.size()) {
            value = arg.substr(position + 1);
            position = arg.size();
            return true;
        }
        if (index + 1 < argc) {
            value = argv[++index];
            position = arg.size();
            return true;
        }
        std::wcerr << L"ioscan: option -" << option << L" requires an argument\n";
        return false;
    }

    bool Parse(int argc, wchar_t* argv[], IoscanOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                OutputFormatter::PrintHelp(argv[0]);
                exitEarly = true;
                return true;
            }

            if (arg == L"--json") { opts.outputFormat = 1; continue; }
            if (arg == L"--csv") { opts.outputFormat = 2; continue; }
            if (arg == L"--table") { opts.outputFormat = 3; continue; }
            if (arg == L"--summary") { opts.summaryView = true; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }
            if (arg == L"--pipe") {
                std::wcerr << L"ioscan: option --pipe requires an argument\n";
                return false;
            }

            if (arg.length() > 1 && (arg[0] == L'-' || arg[0] == L'/')) {
                for (size_t j = 1; j < arg.length(); ++j) {
                    wchar_t c = arg[j];
                    if (c == L'f') opts.fullView = true;
                    else if (c == L'F') opts.compactView = true;
                    else if (c == L'N') opts.agileView = true;
                    else if (c == L'n') opts.listDeviceFiles = true;
                    else if (c == L'A') opts.showAlias = true;
                    else if (c == L'a') opts.processorThreads = true;
                    else if (c == L'B') opts.listPendingBindings = true;
                    else if (c == L'e') opts.showDevicePath = true;
                    else if (c == L'k') { opts.kernelOnly = true; opts.presentOnly = false; }
                    else if (c == L'u') { opts.usableOnly = true; }
                    else if (c == L'l') opts.localOnly = true;
                    else if (c == L's') { opts.staleOnly = true; opts.presentOnly = false; }
                    else if (c == L't') opts.showScanTime = true;
                    else if (c == L'U') opts.unclaimedInterfacesOnly = true;
                    else if (c == L'C' || c == L'd' || c == L'H' || c == L'I' || c == L'm' || c == L'P') {
                        std::wstring value;
                        if (!TakeValue(arg, j, argc, argv, i, c, value)) return false;
                        if (c == L'C') opts.classFilter = DeviceMapper::MapHpUxClassToWindows(value);
                        else if (c == L'd') opts.driverFilter = value;
                        else if (c == L'H') opts.hwPathFilter = value;
                        else if (c == L'm') opts.mappingKeyword = value;
                        else if (c == L'P') opts.propertyName = value;
                        else {
                            try {
                                size_t consumed = 0;
                                opts.instanceFilter = std::stoi(value, &consumed);
                                if (consumed != value.size() || opts.instanceFilter < 0) throw std::invalid_argument("instance");
                            } catch (...) {
                                std::wcerr << L"ioscan: invalid instance number: " << value << L"\n";
                                return false;
                            }
                        }
                        break;
                    } else if (c == L'D' || c == L'M' || c == L'R') {
                        std::wcerr << L"ioscan: -" << c
                                   << L" changes HP-UX kernel driver bindings and is not supported on Windows\n";
                        return false;
                    } else {
                        std::wcerr << L"Unknown flag option: -" << c << L"\nUse -h for help.\n";
                        return false;
                    }
                }
            } else {
                std::wcerr << L"Unexpected argument: " << arg << L"\nUse -h for help.\n";
                return false;
            }
        }

        if (opts.showScanTime && argc != 2) {
            std::wcerr << L"ioscan: -t cannot be combined with other options\n";
            return false;
        }
        if (opts.showAlias && !opts.agileView) {
            std::wcerr << L"ioscan: -A requires -N\n";
            return false;
        }
        if (!opts.propertyName.empty() && !opts.agileView) {
            std::wcerr << L"ioscan: -P requires -N\n";
            return false;
        }
        return true;
    }
};

class IoscanApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        ConsoleManager::ConfigureNativePipes();

        IoscanOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        std::vector<DeviceRecord> devices;
        std::unordered_map<std::wstring, int> summaryCount;

        if (!DeviceEnumerator::Enumerate(opts, devices, summaryCount)) {
            return 1;
        }

        return OutputFormatter::RenderOutput(opts, devices, summaryCount);
    }
};

int wmain(int argc, wchar_t* argv[]) {
    IoscanApplication app;
    return app.Run(argc, argv);
}