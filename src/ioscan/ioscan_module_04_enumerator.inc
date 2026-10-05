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

