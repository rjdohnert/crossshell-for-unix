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

