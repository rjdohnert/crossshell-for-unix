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
 * SINGLE FILE INDEX: driverctl.cpp
 * ============================================================================
 * Driverctl - Windows Device & Driver Diagnostic Controller
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [FORMATTING & ESCAPE HELPERS] ......... Fmt/JoinList + JSON/CSV escapers
 * 2. [STRING & PRIVILEGE UTILITIES] ........ ToUtf8 / IsRunningElevated (TokenElevation)
 * 3. [DATA MODELS] ......................... HealthStatus enum, DeviceRecord struct
 * 4. [PROBLEM CODE DECODER] ................ ProblemDecoder (CM_PROB_* -> text)
 * 5. [REPORT LOGGER] ....................... ReportLogger (timestamped log in ~/driverctl-logs)
 * 6. [DEVICE SCANNER ENGINE] ............... DeviceScanner (SetupAPI/CfgMgr32, registry
 *                                            service-typing, upper/lower filter capture)
 * 7. [APPLICATION CONTROLLER] .............. DriverCtlApp (CLI parsing, Report/Table/JSON/CSV
 *                                            renderers, stdin pipeline filters) + main()
 * ============================================================================
 */

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devpkey.h>
#include <knownfolders.h>
#include <shlobj.h>

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <chrono>
#include <fstream>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <cctype>
#include <cstdio>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace fs = std::filesystem;

// C++17-compatible replacement for std::format with sequential {} placeholders.
// Writes into a single reusable grow-only buffer instead of constructing an
// std::ostringstream (with its locale machinery + internal allocation) per call.
namespace detail {
    // Thread-local reusable output buffer; grows to the largest line and is
    // reused for every Fmt call, eliminating per-line heap churn.
    inline std::string& FmtBuffer() {
        thread_local std::string buf;
        return buf;
    }

    // Append a single argument to the buffer. Strings copy directly; integrals
    // format via snprintf into a small stack scratch (no iostream involved).
    inline void AppendArg(std::string& out, const std::string& v) { out += v; }
    inline void AppendArg(std::string& out, std::string_view v)     { out.append(v.data(), v.size()); }
    inline void AppendArg(std::string& out, const char* v)          { out += v; }
    inline void AppendArg(std::string& out, char v)                 { out += v; }

    template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value, int>::type = 0>
    inline void AppendArg(std::string& out, T v) {
        char scratch[32];
        int n = 0;
        if constexpr (std::is_signed<T>::value)
            n = snprintf(scratch, sizeof(scratch), "%lld", static_cast<long long>(v));
        else
            n = snprintf(scratch, sizeof(scratch), "%llu", static_cast<unsigned long long>(v));
        if (n > 0) out.append(scratch, static_cast<size_t>(n));
    }

    inline void FmtImpl(std::string& out, const char* fmt) {
        out += fmt; // no remaining args; emit the rest verbatim
    }
    template <typename T, typename... Rest>
    void FmtImpl(std::string& out, const char* fmt, T&& value, Rest&&... rest) {
        for (const char* p = fmt; *p; ++p) {
            if (p[0] == '{' && p[1] == '}') {
                AppendArg(out, std::forward<T>(value));
                FmtImpl(out, p + 2, std::forward<Rest>(rest)...);
                return;
            }
            out += *p;
        }
    }
}

// Formats into the reusable buffer and returns a reference to it. Callers pass
// the result straight to WriteLine, so no per-call std::string is returned.
// NOTE: the returned view is invalidated by the next Fmt call on this thread.
template <typename... Args>
const std::string& Fmt(const std::string& fmt, Args&&... args) {
    std::string& out = detail::FmtBuffer();
    out.clear();
    detail::FmtImpl(out, fmt.c_str(), std::forward<Args>(args)...);
    return out;
}

// Joins a list of strings with ", " for compact report display.
std::string JoinList(const std::vector<std::string>& items) {
    std::ostringstream oss;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << items[i];
    }
    return oss.str();
}

// Output formats for pipeline-friendly rendering.
enum class OutputFormat {
    Report,   // default human-readable diagnostic report (console + log file)
    Table,    // aligned tabular columns on stdout
    Json,     // JSON array on stdout
    Csv       // CSV rows on stdout
};

// Escapes a string for embedding in a JSON string literal.
std::string EscapeJSON(const std::string& s) {
    std::string res;
    for (char c : s) {
        switch (c) {
            case '\\': res += "\\\\"; break;
            case '"':  res += "\\\""; break;
            case '\n': res += "\\n";  break;
            case '\r': res += "\\r";  break;
            case '\t': res += "\\t";  break;
            default:    res += c;
        }
    }
    return res;
}

// Escapes a field for CSV (wraps in quotes, doubles internal quotes).
std::string EscapeCSV(const std::string& s) {
    std::string res = "\"";
    for (char c : s) {
        if (c == '"') res += "\"\"";
        else res += c;
    }
    res += "\"";
    return res;
}

// Maps a HealthStatus to a stable lowercase string for machine-readable output.
// (defined after the HealthStatus enum below)

// Converts wide strings to standard UTF-8 strings
std::string ToUtf8(std::wstring_view wstr) {
    if (wstr.empty()) return {};
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (sizeNeeded <= 0) return {};
    std::string str(sizeNeeded, 0);
    int written = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), str.data(), sizeNeeded, nullptr, nullptr);
    if (written <= 0) return {};
    str.resize(written);
    return str;
}

// Returns true when the current process token is elevated (UAC-aware).
// Uses TokenElevation rather than the legacy Admin-SID membership test, which
// reports a non-elevated state for split tokens where the Admin SID is
// present but flagged SE_GROUP_USE_FOR_DENY_ONLY.
bool IsRunningElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    bool elevated = false;
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
        elevated = (elevation.TokenIsElevated != 0);
    }
    CloseHandle(token);
    return elevated;
}

// Result of driver status testing
enum class HealthStatus {
    Healthy,
    Warning,
    Malfunction,
    Disabled
};

// Maps a HealthStatus to a stable lowercase string for machine-readable output.
std::string HealthToString(HealthStatus h) {
    switch (h) {
        case HealthStatus::Healthy:     return "healthy";
        case HealthStatus::Warning:     return "warning";
        case HealthStatus::Malfunction: return "malfunction";
        case HealthStatus::Disabled:    return "disabled";
    }
    return "unknown";
}

// Represents an inspected device and its driver metadata
struct DeviceRecord {
    std::string deviceName;
    std::string category;
    std::string driverType;
    std::string driverProvider;
    std::string driverVersion;
    std::wstring serviceName;            // kept native (wide) to avoid a UTF-8 round-trip
    std::vector<std::string> upperFilters; // DEVPKEY_Device_UpperFilters
    std::vector<std::string> lowerFilters; // DEVPKEY_Device_LowerFilters
    HealthStatus health = HealthStatus::Healthy;
    ULONG problemCode = 0;
    std::string diagnosticDetails;
};

// Decodes Windows CM_PROB_* codes into human-readable descriptions
class ProblemDecoder {
public:
    static std::string Decode(ULONG problemCode) {
        switch (problemCode) {
            case 0: return "Device is operating normally (No problem detected).";
            case CM_PROB_NOT_CONFIGURED: return "Code 1: Device is not configured correctly.";
            case CM_PROB_FAILED_START: return "Code 10: Device failed to start (Driver or Hardware malfunction).";
            case CM_PROB_OUT_OF_MEMORY: return "Code 3: Driver ran out of system memory.";
            case CM_PROB_NORMAL_CONFLICT: return "Code 12: Hardware resource conflict detected.";
            case CM_PROB_NEED_RESTART: return "Code 14: System restart required to complete driver initialization.";
            case CM_PROB_REINSTALL: return "Code 18: Drivers for this device must be reinstalled.";
            case CM_PROB_DISABLED: return "Code 22: Device is explicitly disabled by user/system.";
            case CM_PROB_FAILED_INSTALL: return "Code 28: No compatible driver installed for this hardware.";
            case CM_PROB_FAILED_ADD: return "Code 31: Device driver failed to load.";
            case CM_PROB_DISABLED_SERVICE: return "Code 32: The driver service is disabled in the registry.";
            case CM_PROB_DRIVER_FAILED_LOAD: return "Code 39: Driver binary corrupted or missing.";
            case CM_PROB_FAILED_POST_START: return "Code 43: Device reported a critical hardware/firmware failure.";
            default: return Fmt("Uncommon Hardware/Driver Error Code ({})", problemCode);
        }
    }
};

// Handles file logging to the user's home folder
class ReportLogger {
private:
    fs::path logPath;
    std::ofstream logFile;

public:
    ReportLogger() {
        PWSTR userProfile = nullptr;
        fs::path baseDir;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &userProfile))) {
            baseDir = userProfile;
            CoTaskMemFree(userProfile);
        } else {
            // Fall back to the current directory if the profile path is unavailable.
            std::error_code ec;
            baseDir = fs::current_path(ec);
            std::cerr << "[driverctl] Warning: could not resolve user profile folder; "
                      << "writing log to current directory.\n";
        }

        // Store logs in a dedicated "driverctl-logs" directory under the base dir.
        std::error_code ec;
        fs::path logDir = baseDir / "driverctl-logs";
        if (!fs::exists(logDir, ec)) {
            if (!fs::create_directories(logDir, ec) || ec) {
                std::cerr << "[driverctl] Warning: could not create log directory: "
                          << logDir.string() << " (" << ec.message() << "); using base directory.\n";
                logDir = baseDir;
            }
        }

        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_s(&tm, &t);
        std::ostringstream ts;
        ts << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
        std::string filename = "driverctl_diag_" + ts.str() + ".log";
        // Note: opening truncates any existing file of the same name; the
        // timestamp makes collisions unlikely (same-second re-runs only).
        logPath = logDir / filename;
        logFile.open(logPath);
        if (logFile.fail()) {
            std::cerr << "[driverctl] Warning: failed to open log file: "
                      << logPath.string() << "\n";
        }
    }

    void WriteLine(std::string_view line) {
        std::cout << line << "\n";
        if (logFile.is_open()) {
            logFile << line << "\n";
        }
    }

    fs::path GetPath() const { return logPath; }
};

// Core scanner responsible for inspecting devices and testing drivers
class DeviceScanner {
public:
    std::vector<DeviceRecord> Scan(bool testDrivers = true, std::string_view categoryFilter = "") {
        std::vector<DeviceRecord> results;

        HDEVINFO devInfo = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (devInfo == INVALID_HANDLE_VALUE) return results;

        SP_DEVINFO_DATA devData{};
        devData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(devInfo, i, &devData); ++i) {
            DeviceRecord rec;
            rec.deviceName = GetDeviceStringProperty(devInfo, devData, DEVPKEY_Device_DeviceDesc);
            if (rec.deviceName.empty()) {
                rec.deviceName = GetDeviceStringProperty(devInfo, devData, DEVPKEY_Device_FriendlyName);
            }
            if (rec.deviceName.empty()) rec.deviceName = "Unknown Hardware Node";

            rec.category = GetDeviceStringProperty(devInfo, devData, DEVPKEY_Device_Class);
            if (rec.category.empty()) rec.category = "System / Unspecified";

            if (!categoryFilter.empty()) {
                std::string catLower = rec.category;
                std::string filterLower = std::string(categoryFilter);
                std::transform(catLower.begin(), catLower.end(), catLower.begin(), ::tolower);
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
                if (catLower.find(filterLower) == std::string::npos) continue;
            }

            rec.driverProvider = GetDeviceStringProperty(devInfo, devData, DEVPKEY_Device_DriverProvider);
            rec.driverVersion  = GetDeviceStringProperty(devInfo, devData, DEVPKEY_Device_DriverVersion);
            rec.serviceName    = GetDeviceWideProperty(devInfo, devData, DEVPKEY_Device_Service);

            // Capture the WDM filter stack so upper/lower filter drivers (AV,
            // disk encryption, HID filters, etc.) are visible in diagnostics.
            rec.upperFilters   = GetDeviceStringListProperty(devInfo, devData, DEVPKEY_Device_UpperFilters);
            rec.lowerFilters   = GetDeviceStringListProperty(devInfo, devData, DEVPKEY_Device_LowerFilters);

            // Determine driver architecture type (filter stack informs UMDF detection)
            rec.driverType = DetermineDriverType(rec.serviceName, rec.category, rec.upperFilters, rec.lowerFilters);

            // Run Hardware & Driver Diagnostic Test
            if (testDrivers) {
                RunDriverDiagnostics(devData.DevInst, rec);
            }

            results.push_back(std::move(rec));
        }

        SetupDiDestroyDeviceInfoList(devInfo);
        return results;
    }

private:
    // Classify a driver by reading its service Type from the registry
    // (HKLM\SYSTEM\CurrentControlSet\Services\<name>) instead of making a
    // per-device RPC round-trip to the Service Control Manager.
    // UMDF hosts are detected by the presence of the WUDF reflector (wudfrd)
    // in the device's filter stack, NOT by the generic Win32 service type bit.
    std::string DetermineDriverType(const std::wstring& service, const std::string& category,
                                    const std::vector<std::string>& upperFilters,
                                    const std::vector<std::string>& lowerFilters) {
        if (service.empty()) return "Standard PnP / Bus Enumerated";

        // A true UMDF driver runs in WUDFHost.exe, bound via wudfrd.sys as a
        // filter. Detect it from the filter stack we already captured. Uses a
        // non-allocating case-insensitive substring search (no lowercase copy).
        auto containsCaseInsensitive = [](const std::string& haystack, const char* needleLower) {
            const size_t n = haystack.size();
            const size_t m = strlen(needleLower);
            if (m == 0 || n < m) return false;
            for (size_t i = 0; i + m <= n; ++i) {
                size_t j = 0;
                for (; j < m; ++j) {
                    unsigned char a = static_cast<unsigned char>(haystack[i + j]);
                    if (tolower(a) != (unsigned char)needleLower[j]) break;
                }
                if (j == m) return true;
            }
            return false;
        };
        auto hasWudfReflector = [&](const std::vector<std::string>& v) {
            for (const auto& f : v) {
                if (containsCaseInsensitive(f, "wudfrd")) return true;
            }
            return false;
        };
        if (hasWudfReflector(upperFilters) || hasWudfReflector(lowerFilters)) {
            return "User-Mode Driver (UMDF Host)";
        }

        std::wstring keyPath = L"SYSTEM\\CurrentControlSet\\Services\\" + service;
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
            return "Kernel Driver Service";
        }
        DWORD type = 0;
        DWORD dataSize = sizeof(type);
        DWORD valueType = 0;
        LONG rc = RegQueryValueExW(hKey, L"Type", nullptr, &valueType,
                                   reinterpret_cast<LPBYTE>(&type), &dataSize);
        RegCloseKey(hKey);
        if (rc != ERROR_SUCCESS || valueType != REG_DWORD) return "Kernel Driver Service";

        if (type & SERVICE_KERNEL_DRIVER)        return "Kernel-Mode Driver (KMDF / WDM)";
        if (type & SERVICE_FILE_SYSTEM_DRIVER)   return "File System Filter Driver";
        // SERVICE_WIN32_OWN_PROCESS / SHARE_PROCESS are ordinary Win32 services,
        // not UMDF hosts. Label them accurately.
        if (type & (SERVICE_WIN32_OWN_PROCESS | SERVICE_WIN32_SHARE_PROCESS)) return "Win32 Service";
        return "Kernel Driver Service";
    }

    void RunDriverDiagnostics(DEVINST devInst, DeviceRecord& rec) {
        ULONG status = 0;
        ULONG probNum = 0;
        CONFIGRET cr = CM_Get_DevNode_Status(&status, &probNum, devInst, 0);

        if (cr != CR_SUCCESS) {
            rec.health = HealthStatus::Warning;
            rec.diagnosticDetails = "Unable to query device node status flags.";
            return;
        }

        if (status & DN_HAS_PROBLEM) {
            rec.problemCode = probNum;
            if (probNum == CM_PROB_DISABLED) {
                rec.health = HealthStatus::Disabled;
            } else {
                rec.health = HealthStatus::Malfunction;
            }
            rec.diagnosticDetails = ProblemDecoder::Decode(probNum);
        } else if (!(status & DN_STARTED)) {
            // No problem code, but the devnode isn't started. This is often normal
            // (disabled device, stopped bus, or a node that simply hasn't been
            // started by its bus driver yet), so it is informational, not a warning.
            rec.health = HealthStatus::Healthy;
            rec.diagnosticDetails = "Device node not started (no problem reported by Configuration Manager).";
        } else {
            rec.health = HealthStatus::Healthy;
            rec.diagnosticDetails = "Device node active; driver responsive and operating normally.";
        }
    }

    // Reusable grow-only scratch buffer for property reads. Avoids allocating a
    // fresh heap block per property per devnode (hundreds of nodes x ~8 props).
    std::vector<BYTE> m_scratch;

    // Reads a device property into the shared scratch buffer. Returns the byte
    // count actually written (0 on failure). Sets *propTypeOut when non-null.
    DWORD ReadPropertyIntoScratch(HDEVINFO devInfo, const SP_DEVINFO_DATA& devData,
                                  const DEVPROPKEY& key, DEVPROPTYPE* propTypeOut) {
        DEVPROPTYPE propType = 0;
        DWORD size = 0;
        SetupDiGetDevicePropertyW(devInfo, const_cast<PSP_DEVINFO_DATA>(&devData), &key, &propType, nullptr, 0, &size, 0);
        if (size == 0) return 0;
        if (m_scratch.size() < size) m_scratch.resize(size);
        if (!SetupDiGetDevicePropertyW(devInfo, const_cast<PSP_DEVINFO_DATA>(&devData), &key, &propType, m_scratch.data(), size, &size, 0)) {
            return 0;
        }
        if (propTypeOut) *propTypeOut = propType;
        return size;
    }

    std::string GetDeviceStringProperty(HDEVINFO devInfo, const SP_DEVINFO_DATA& devData, const DEVPROPKEY& key) {
        DEVPROPTYPE propType;
        DWORD size = ReadPropertyIntoScratch(devInfo, devData, key, &propType);
        if (size != 0 && propType == DEVPROP_TYPE_STRING) {
            return ToUtf8(reinterpret_cast<PCWSTR>(m_scratch.data()));
        }
        return {};
    }

    // Same as GetDeviceStringProperty but returns the native wide string (no
    // UTF-8 conversion), for values that feed back into wide Win32 APIs.
    std::wstring GetDeviceWideProperty(HDEVINFO devInfo, const SP_DEVINFO_DATA& devData, const DEVPROPKEY& key) {
        DEVPROPTYPE propType;
        DWORD size = ReadPropertyIntoScratch(devInfo, devData, key, &propType);
        if (size != 0 && propType == DEVPROP_TYPE_STRING) {
            return reinterpret_cast<PCWSTR>(m_scratch.data());
        }
        return {};
    }

    // Reads a DEVPROP_TYPE_STRING_LIST property (REG_MULTI_SZ) into a vector of
    // UTF-8 strings.
    std::vector<std::string> GetDeviceStringListProperty(HDEVINFO devInfo, const SP_DEVINFO_DATA& devData, const DEVPROPKEY& key) {
        std::vector<std::string> out;
        DEVPROPTYPE propType;
        DWORD size = ReadPropertyIntoScratch(devInfo, devData, key, &propType);
        if (size == 0 || propType != DEVPROP_TYPE_STRING_LIST) return out;

        // String list is a sequence of NUL-terminated wide strings, double-NUL ended.
        PCWSTR p = reinterpret_cast<PCWSTR>(m_scratch.data());
        PCWSTR end = reinterpret_cast<PCWSTR>(m_scratch.data() + size);
        while (p < end && *p) {
            std::wstring entry(p);
            if (!entry.empty()) out.push_back(ToUtf8(entry));
            p += entry.size() + 1;
        }
        return out;
    }
};

// Application Controller & CLI Interface
class DriverCtlApp {
private:
    DeviceScanner scanner;
    // Lazily constructed so structured output paths (--json/--csv/--table) never
    // touch the disk. Only created when the human-readable Report path runs.
    std::unique_ptr<ReportLogger> logger;

    // Renders records as an aligned table on stdout (pipeline/parse friendly).
    static void RenderTable(const std::vector<DeviceRecord>& records, bool runTest) {
        auto join = [](const std::vector<std::string>& v) { return v.empty() ? std::string("-") : JoinList(v); };
        std::cout << std::left
                  << std::setw(32) << "Device Name" << "  "
                  << std::setw(18) << "Category" << "  "
                  << std::setw(28) << "Driver Type" << "  "
                  << std::setw(20) << "Service" << "  "
                  << std::setw(20) << "Upper Filters" << "  "
                  << std::setw(20) << "Lower Filters";
        if (runTest) std::cout << "  " << std::setw(12) << "Health";
        std::cout << "\n";

        for (const auto& rec : records) {
            std::cout << std::left
                      << std::setw(32) << rec.deviceName.substr(0, 31) << "  "
                      << std::setw(18) << rec.category.substr(0, 17) << "  "
                      << std::setw(28) << rec.driverType.substr(0, 27) << "  "
                      << std::setw(20) << (rec.serviceName.empty() ? "-" : ToUtf8(rec.serviceName)).substr(0, 19) << "  "
                      << std::setw(20) << join(rec.upperFilters).substr(0, 19) << "  "
                      << std::setw(20) << join(rec.lowerFilters).substr(0, 19);
            if (runTest) std::cout << "  " << std::setw(12) << HealthToString(rec.health);
            std::cout << "\n";
        }
    }

    // Renders records as a JSON array on stdout.
    static void RenderJSON(const std::vector<DeviceRecord>& records, bool runTest) {
        std::cout << "[\n";
        for (size_t i = 0; i < records.size(); ++i) {
            const auto& rec = records[i];
            std::cout << "  {\n";
            std::cout << "    \"deviceName\": \"" << EscapeJSON(rec.deviceName) << "\",\n";
            std::cout << "    \"category\": \"" << EscapeJSON(rec.category) << "\",\n";
            std::cout << "    \"driverType\": \"" << EscapeJSON(rec.driverType) << "\",\n";
            std::cout << "    \"driverProvider\": \"" << EscapeJSON(rec.driverProvider) << "\",\n";
            std::cout << "    \"driverVersion\": \"" << EscapeJSON(rec.driverVersion) << "\",\n";
            std::cout << "    \"serviceName\": \"" << EscapeJSON(ToUtf8(rec.serviceName)) << "\",\n";

            auto jsonList = [](const std::vector<std::string>& v) {
                std::string s = "[";
                for (size_t k = 0; k < v.size(); ++k) {
                    if (k > 0) s += ", ";
                    s += "\"" + EscapeJSON(v[k]) + "\"";
                }
                s += "]";
                return s;
            };
            std::cout << "    \"upperFilters\": " << jsonList(rec.upperFilters) << ",\n";
            std::cout << "    \"lowerFilters\": " << jsonList(rec.lowerFilters);

            if (runTest) {
                std::cout << ",\n";
                std::cout << "    \"health\": \"" << HealthToString(rec.health) << "\",\n";
                std::cout << "    \"problemCode\": " << rec.problemCode << ",\n";
                std::cout << "    \"diagnosticDetails\": \"" << EscapeJSON(rec.diagnosticDetails) << "\"\n";
            } else {
                std::cout << "\n";
            }
            std::cout << "  }" << (i + 1 < records.size() ? "," : "") << "\n";
        }
        std::cout << "]\n";
    }

    // Renders records as CSV on stdout (header + one row per device).
    static void RenderCSV(const std::vector<DeviceRecord>& records, bool runTest) {
        std::cout << "DeviceName,Category,DriverType,DriverProvider,DriverVersion,ServiceName,UpperFilters,LowerFilters";
        if (runTest) std::cout << ",Health,ProblemCode,DiagnosticDetails";
        std::cout << "\n";

        for (const auto& rec : records) {
            // Multi-value filter lists are joined with ';' inside a quoted cell.
            auto joinSemi = [](const std::vector<std::string>& v) {
                std::string s;
                for (size_t k = 0; k < v.size(); ++k) { if (k > 0) s += ";"; s += v[k]; }
                return s;
            };
            std::cout << EscapeCSV(rec.deviceName) << ","
                      << EscapeCSV(rec.category) << ","
                      << EscapeCSV(rec.driverType) << ","
                      << EscapeCSV(rec.driverProvider) << ","
                      << EscapeCSV(rec.driverVersion) << ","
                      << EscapeCSV(ToUtf8(rec.serviceName)) << ","
                      << EscapeCSV(joinSemi(rec.upperFilters)) << ","
                      << EscapeCSV(joinSemi(rec.lowerFilters));
            if (runTest) {
                std::cout << "," << EscapeCSV(HealthToString(rec.health))
                          << "," << rec.problemCode
                          << "," << EscapeCSV(rec.diagnosticDetails);
            }
            std::cout << "\n";
        }
    }

    // True when stdin is redirected (piped), so we can read category filters.
    static bool IsInputPiped() {
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        if (h == nullptr || h == INVALID_HANDLE_VALUE) return false;
        return GetFileType(h) != FILE_TYPE_CHAR; // not a console => pipe/file
    }

    // Reads only the first whitespace-delimited token from stdin. We never use
    // more than one category filter, so avoid slurping a large piped stream into
    // a vector just to keep the first element.
    static std::string ReadPipedFirstFilter() {
        std::string token;
        if (std::cin >> token) return token;
        return "";
    }

public:
    void ShowHelp() {
        std::cout << R"(driverctl(1)               CrossShell for UNIX Reference Manual               driverctl(1)

    NAME
        driverctl - inspect and diagnose Windows PnP device drivers

    SYNOPSIS
        driverctl [COMMAND] [OPTIONS]

    DESCRIPTION
        Interfaces directly with the Windows PnP Configuration Manager to inspect
        device drivers, diagnose hardware malfunctions, start errors (Code 10),
        critical failures (Code 43), and uninstalled or corrupted states.

        When stdin is piped, driverctl reads additional category filters from stdin.

    OPTIONS
        --list
            Enumerate all installed devices, categories, and drivers.
        --test
            Run live hardware and driver malfunction diagnostics.
        --category NAME
            Filter devices by setup class/category (e.g., Display, Net, USB).
        --errors-only
            Suppress healthy devices; only report warnings and failures.
        --table
            Emit results as an aligned table on stdout.
        --json
            Emit results as a JSON array on stdout.
        --csv
            Emit results as CSV on stdout.
        -h, --help
            Display this reference manual and exit.

    EXAMPLES
        driverctl --list
            Enumerate all installed devices and drivers.

        driverctl --test --category Display
            Inspect all display/graphics drivers and test for malfunctions.

        driverctl --test --errors-only
            Run full diagnostic and only display failed devices.

        driverctl --list --json
            Export device inventory formatted as JSON.

    CrossShell for UNIX                                                    driverctl(1)
)";
    }

    int Run(int argc, char* argv[]) {
        bool runTest = false;
        bool listOnly = false;
        bool errorsOnly = false;
        OutputFormat format = OutputFormat::Report;
        std::string categoryFilter;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                ShowHelp();
                return 0;
            } else if (arg == "--test") {
                runTest = true;
            } else if (arg == "--list") {
                listOnly = true;
            } else if (arg == "--errors-only") {
                errorsOnly = true;
            } else if (arg == "--table") {
                format = OutputFormat::Table;
            } else if (arg == "--json") {
                format = OutputFormat::Json;
            } else if (arg == "--csv") {
                format = OutputFormat::Csv;
            } else if (arg == "--category" && i + 1 < argc) {
                categoryFilter = argv[++i];
            }
        }

        // Pipeline support: when stdin is piped, take the first token as a
        // category filter. An explicit --category always wins.
        if (categoryFilter.empty() && IsInputPiped()) {
            categoryFilter = ReadPipedFirstFilter();
        }

        if (!runTest && !listOnly) {
            ShowHelp();
            return 0;
        }

        // Structured formats render clean data to stdout and skip the report/log
        // banner so downstream tools receive parseable output only.
        if (format != OutputFormat::Report) {
            auto records = scanner.Scan(runTest, categoryFilter);
            if (errorsOnly) {
                records.erase(std::remove_if(records.begin(), records.end(),
                    [](const DeviceRecord& r) { return r.health == HealthStatus::Healthy; }),
                    records.end());
            }
            switch (format) {
                case OutputFormat::Table: RenderTable(records, runTest); break;
                case OutputFormat::Json:  RenderJSON(records, runTest);  break;
                case OutputFormat::Csv:   RenderCSV(records, runTest);   break;
                default: break;
            }
            return 0;
        }

        // Human-readable report path: only now do we construct the logger, which
        // performs disk I/O (creates ~/driverctl-logs and opens the log file).
        logger = std::make_unique<ReportLogger>();

        logger->WriteLine("");
        logger->WriteLine("                      Driverctl Device Diagnostic Report                      ");
        logger->WriteLine("");
        if (!IsRunningElevated()) {
            logger->WriteLine("NOTE: Not running as administrator - some device status and problem");
            logger->WriteLine("      codes may be incomplete. Re-run elevated for full diagnostics.");
            logger->WriteLine("");
        }
        logger->WriteLine(Fmt("Log Path: {}", logger->GetPath().string()));
        if (!categoryFilter.empty()) logger->WriteLine(Fmt("Filter: Category matching '{}'", categoryFilter));
        logger->WriteLine("\n");

        auto records = scanner.Scan(runTest, categoryFilter);

        size_t healthyCount = 0, warningCount = 0, errorCount = 0;

        for (const auto& rec : records) {
            if (rec.health == HealthStatus::Healthy) healthyCount++;
            else if (rec.health == HealthStatus::Warning) warningCount++;
            else if (rec.health == HealthStatus::Malfunction) errorCount++;

            if (errorsOnly && rec.health == HealthStatus::Healthy) continue;

            logger->WriteLine(Fmt("Device Name     : {}", rec.deviceName));
            logger->WriteLine(Fmt("Category        : {}", rec.category));
            logger->WriteLine(Fmt("Driver Type     : {}", rec.driverType));
            logger->WriteLine(Fmt("Driver Provider : {}", rec.driverProvider.empty() ? "N/A" : rec.driverProvider));
            logger->WriteLine(Fmt("Driver Version  : {}", rec.driverVersion.empty() ? "N/A" : rec.driverVersion));
            logger->WriteLine(Fmt("Service Name    : {}", rec.serviceName.empty() ? "None (Direct/Bus)" : ToUtf8(rec.serviceName)));

            if (!rec.upperFilters.empty()) {
                logger->WriteLine(Fmt("Upper Filters   : {}", JoinList(rec.upperFilters)));
            }
            if (!rec.lowerFilters.empty()) {
                logger->WriteLine(Fmt("Lower Filters   : {}", JoinList(rec.lowerFilters)));
            }

            if (runTest) {
                std::string statusTag;
                switch (rec.health) {
                    case HealthStatus::Healthy:     statusTag = "[PASS] HEALTHY"; break;
                    case HealthStatus::Warning:     statusTag = "[WARN] WARNING"; break;
                    case HealthStatus::Malfunction: statusTag = "[FAIL] MALFUNCTION DETECTED"; break;
                    case HealthStatus::Disabled:    statusTag = "[INFO] DISABLED"; break;
                }
                logger->WriteLine(Fmt("Test Verdict    : {}", statusTag));
                logger->WriteLine(Fmt("Diagnostic Note : {}", rec.diagnosticDetails));
            }
            logger->WriteLine("-------------------------------------------------------------------------------");
        }

        logger->WriteLine("\n");
        logger->WriteLine("                               Diagnostic Summary                            ");
        logger->WriteLine("");
        logger->WriteLine(Fmt("Total Devices Scanned : {}", records.size()));
        logger->WriteLine(Fmt("Operational / Healthy : {}", healthyCount));
        logger->WriteLine(Fmt("Warnings / Deferred   : {}", warningCount));
        logger->WriteLine(Fmt("Malfunctions / Errors : {}", errorCount));
        logger->WriteLine("\n");
        logger->WriteLine(Fmt("Complete report saved to: {}", logger->GetPath().string()));

        return 0;
    }
};

int main(int argc, char* argv[]) {
    DriverCtlApp app;
    return app.Run(argc, argv);
}