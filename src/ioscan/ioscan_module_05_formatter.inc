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

