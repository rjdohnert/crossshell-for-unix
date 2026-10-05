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
