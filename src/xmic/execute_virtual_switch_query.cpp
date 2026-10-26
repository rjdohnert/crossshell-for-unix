#include "xmic_common.hpp"

bool ExecuteVirtualSwitchQuery(const WmiTargetConfig& target, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    g_lastNoData = false;

    Table table;
    table.headers = { L"Category", L"Item", L"Value" };

    auto addRow = [&](const std::wstring& category, const std::wstring& item, const std::wstring& value) {
        table.rows.push_back({ category, item, value });
    };

    addRow(L"Context", L"Process Elevation", GetProcessElevationStateString());
    addRow(L"Context", L"User SID", GetCurrentUserSidString());
    if (g_outputOptions.verbose) {
        addRow(L"Context", L"Verbose", L"Enabled");
    }

    std::wstring wslPlatformState;
    std::vector<std::wstring> wslDiagnostics;
    auto addDiagnostic = [&](const std::wstring& message) {
        if (!message.empty()) {
            wslDiagnostics.push_back(message);
        }
    };

    auto updatePlatformRow = [&](const std::wstring& item, const std::wstring& value) {
        for (auto& row : table.rows) {
            if (row.size() >= 2 && row[0] == L"Platform" && row[1] == item) {
                row[2] = value;
            }
        }
    };

    IWbemServices* pSvc = NULL;
    if (ConnectWmiServices(target, L"ROOT\\CIMV2", securityConfig, wmiLocale, &pSvc)) {
        std::map<std::wstring, std::wstring> featureStates;

        IEnumWbemClassObject* pFeatureEnum = NULL;
        HRESULT hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t(L"SELECT Name, InstallState FROM Win32_OptionalFeature WHERE Name='Microsoft-Hyper-V-All' OR Name='Microsoft-Hyper-V' OR Name='Microsoft-Hyper-V-Hypervisor' OR Name='Microsoft-Hyper-V-Services' OR Name='Microsoft-Hyper-V-Management-PowerShell' OR Name='VirtualMachinePlatform' OR Name='Microsoft-Windows-Subsystem-Linux' OR Name='HypervisorPlatform'"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pFeatureEnum
        );

        if (SUCCEEDED(hr) && pFeatureEnum) {
            IWbemClassObject* pclsObj = NULL;
            ULONG uReturn = 0;
            while (pFeatureEnum) {
                hr = pFeatureEnum->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                if (FAILED(hr) || uReturn == 0) break;

                VARIANT vtName;
                VARIANT vtState;
                VariantInit(&vtName);
                VariantInit(&vtState);

                std::wstring featureName = L"N/A";
                std::wstring state = L"N/A";

                if (SUCCEEDED(pclsObj->Get(L"Name", 0, &vtName, 0, 0))) {
                    featureName = VariantToString(vtName);
                }
                if (SUCCEEDED(pclsObj->Get(L"InstallState", 0, &vtState, 0, 0))) {
                    state = MapOptionalFeatureInstallState(VariantToString(vtState));
                }

                featureStates[featureName] = state;

                if (featureName == L"Microsoft-Hyper-V-All") featureName = L"Hyper-V";
                else if (featureName == L"Microsoft-Hyper-V") featureName = L"Hyper-V Base";
                else if (featureName == L"Microsoft-Hyper-V-Hypervisor") featureName = L"Hyper-V Hypervisor";
                else if (featureName == L"Microsoft-Hyper-V-Services") featureName = L"Hyper-V Services";
                else if (featureName == L"Microsoft-Hyper-V-Management-PowerShell") featureName = L"Hyper-V PowerShell";
                else if (featureName == L"VirtualMachinePlatform") featureName = L"Virtual Machine Platform";
                else if (featureName == L"Microsoft-Windows-Subsystem-Linux") featureName = L"Windows Subsystem for Linux";
                else if (featureName == L"HypervisorPlatform") featureName = L"Windows Hypervisor Platform";

                addRow(L"Platform", featureName, state);

                VariantClear(&vtName);
                VariantClear(&vtState);
                pclsObj->Release();
            }

            pFeatureEnum->Release();
        }

        auto aggregateFeatureState = [&](const std::vector<std::wstring>& names) -> std::wstring {
            bool sawDisabled = false;
            for (const auto& n : names) {
                auto it = featureStates.find(n);
                if (it == featureStates.end()) {
                    continue;
                }
                if (it->second == L"Enabled") return L"Enabled";
                if (it->second == L"Disabled") sawDisabled = true;
            }
            if (sawDisabled) return L"Disabled";
            return L"Unavailable";
        };

        addRow(
            L"Platform",
            L"Hyper-V (Aggregated)",
            aggregateFeatureState({ L"Microsoft-Hyper-V-All", L"Microsoft-Hyper-V", L"Microsoft-Hyper-V-Hypervisor", L"Microsoft-Hyper-V-Services" })
        );
        wslPlatformState = aggregateFeatureState({ L"Microsoft-Windows-Subsystem-Linux", L"VirtualMachinePlatform" });
        addRow(
            L"Platform",
            L"WSL (Aggregated)",
            wslPlatformState
        );

        IEnumWbemClassObject* pSysEnum = NULL;
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t(L"SELECT HypervisorPresent FROM Win32_ComputerSystem"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pSysEnum
        );

        if (SUCCEEDED(hr) && pSysEnum) {
            IWbemClassObject* pclsObj = NULL;
            ULONG uReturn = 0;
            hr = pSysEnum->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (SUCCEEDED(hr) && uReturn > 0 && pclsObj) {
                VARIANT vtHypervisor;
                VariantInit(&vtHypervisor);
                std::wstring present = L"N/A";
                if (SUCCEEDED(pclsObj->Get(L"HypervisorPresent", 0, &vtHypervisor, 0, 0))) {
                    present = VariantToString(vtHypervisor);
                }
                addRow(L"Platform", L"Hypervisor Present", present);
                VariantClear(&vtHypervisor);
                pclsObj->Release();
            }
            pSysEnum->Release();
        }

        IEnumWbemClassObject* pSvcEnum = NULL;
        hr = pSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t(L"SELECT Name, State, StartMode FROM Win32_Service WHERE Name='vmms' OR Name='vmcompute' OR Name='LxssManager'"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pSvcEnum
        );

        if (SUCCEEDED(hr) && pSvcEnum) {
            IWbemClassObject* pclsObj = NULL;
            ULONG uReturn = 0;
            while (pSvcEnum) {
                hr = pSvcEnum->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                if (FAILED(hr) || uReturn == 0) break;

                VARIANT vtName;
                VARIANT vtState;
                VARIANT vtStart;
                VariantInit(&vtName);
                VariantInit(&vtState);
                VariantInit(&vtStart);

                std::wstring name = L"N/A";
                std::wstring state = L"N/A";
                std::wstring startMode = L"N/A";

                if (SUCCEEDED(pclsObj->Get(L"Name", 0, &vtName, 0, 0))) name = VariantToString(vtName);
                if (SUCCEEDED(pclsObj->Get(L"State", 0, &vtState, 0, 0))) state = VariantToString(vtState);
                if (SUCCEEDED(pclsObj->Get(L"StartMode", 0, &vtStart, 0, 0))) startMode = VariantToString(vtStart);

                std::wstring display = name;
                if (_wcsicmp(name.c_str(), L"vmms") == 0) display = L"Hyper-V Virtual Machine Management (vmms)";
                else if (_wcsicmp(name.c_str(), L"vmcompute") == 0) display = L"Host Compute Service (vmcompute)";
                else if (_wcsicmp(name.c_str(), L"LxssManager") == 0) display = L"WSL Service (LxssManager)";

                addRow(L"Service", display, state + L" / " + startMode);

                VariantClear(&vtName);
                VariantClear(&vtState);
                VariantClear(&vtStart);
                pclsObj->Release();
            }
            pSvcEnum->Release();
        }

        pSvc->Release();
    } else {
        addRow(L"Platform", L"Virtualization/WSL Feature Detection", L"Unavailable");
        if (g_outputOptions.verbose) {
            addRow(L"WSL Diagnostics", L"Error", L"Unable to connect to ROOT\\CIMV2 for platform feature detection.");
        }
    }

    IWbemServices* pHyperVSvc = NULL;
    if (ConnectWmiServices(target, L"ROOT\\virtualization\\v2", securityConfig, wmiLocale, &pHyperVSvc, false)) {
        IEnumWbemClassObject* pVmEnum = NULL;
        HRESULT hr = pHyperVSvc->ExecQuery(
            bstr_t("WQL"),
            bstr_t(L"SELECT ElementName, EnabledState FROM Msvm_ComputerSystem WHERE Caption='Virtual Machine'"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL,
            &pVmEnum
        );

        int vmCount = 0;
        if (SUCCEEDED(hr) && pVmEnum) {
            IWbemClassObject* pclsObj = NULL;
            ULONG uReturn = 0;
            while (pVmEnum) {
                hr = pVmEnum->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
                if (FAILED(hr) || uReturn == 0) break;

                VARIANT vtName;
                VARIANT vtState;
                VariantInit(&vtName);
                VariantInit(&vtState);

                std::wstring vmName = L"N/A";
                std::wstring vmState = L"N/A";

                if (SUCCEEDED(pclsObj->Get(L"ElementName", 0, &vtName, 0, 0))) {
                    vmName = VariantToString(vtName);
                }
                if (SUCCEEDED(pclsObj->Get(L"EnabledState", 0, &vtState, 0, 0))) {
                    vmState = MapHyperVEnabledState(VariantToString(vtState));
                }

                addRow(L"Hyper-V VM", vmName, vmState);
                ++vmCount;

                VariantClear(&vtName);
                VariantClear(&vtState);
                pclsObj->Release();
            }

            pVmEnum->Release();
        }

        if (vmCount == 0) {
            addRow(L"Hyper-V VM", L"Instances", L"None found");
        }

        pHyperVSvc->Release();
    } else {
        addRow(L"Hyper-V VM", L"Instances", L"Unavailable (namespace not present or access denied)");
        if (g_outputOptions.verbose) {
            addRow(L"WSL Diagnostics", L"Error", L"Unable to connect to ROOT\\virtualization\\v2 for Hyper-V VM enumeration.");
        }
    }

    IWbemServices* pRegSvc = NULL;
    if (ConnectWmiServices(target, L"ROOT\\DEFAULT", securityConfig, wmiLocale, &pRegSvc)) {
        IWbemClassObject* pRegClass = NULL;
        HRESULT hr = pRegSvc->GetObject(_bstr_t(L"StdRegProv"), 0, NULL, &pRegClass, NULL);

        if (SUCCEEDED(hr) && pRegClass != NULL) {
            const unsigned long HKEY_CURRENT_USER_HIVE = 0x80000001;
            const std::wstring lxssPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Lxss";

            std::vector<std::wstring> distroKeys;
            std::wstring defaultDistroGuid = ReadRegistryStringValueWithHive(
                pRegSvc,
                pRegClass,
                HKEY_CURRENT_USER_HIVE,
                lxssPath,
                L"DefaultDistribution"
            );

            int distroCount = 0;
            if (EnumRegistrySubKeysWithHive(pRegSvc, pRegClass, HKEY_CURRENT_USER_HIVE, lxssPath, distroKeys)) {
                for (const auto& key : distroKeys) {
                    std::wstring subKey = lxssPath + L"\\" + key;
                    std::wstring distroName = ReadRegistryStringValueWithHive(
                        pRegSvc,
                        pRegClass,
                        HKEY_CURRENT_USER_HIVE,
                        subKey,
                        L"DistributionName"
                    );

                    if (distroName.empty()) {
                        continue;
                    }

                    unsigned long version = 0;
                    std::wstring value = L"Version Unknown";
                    if (ReadRegistryDwordValueWithHive(pRegSvc, pRegClass, HKEY_CURRENT_USER_HIVE, subKey, L"Version", version)) {
                        value = L"WSL" + std::to_wstring(version);
                    }

                    if (!defaultDistroGuid.empty() && _wcsicmp(defaultDistroGuid.c_str(), key.c_str()) == 0) {
                        value += L" (Default)";
                    }

                    addRow(L"WSL Distro", distroName, value);
                    ++distroCount;
                }
            }

            if (distroCount == 0) {
                std::vector<std::pair<std::wstring, std::wstring>> directDistros;
                if (EnumerateWslDistrosFromRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Lxss", directDistros) ||
                    EnumerateWslDistrosFromRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Lxss", directDistros)) {
                    for (const auto& entry : directDistros) {
                        addRow(L"WSL Distro", entry.first, entry.second);
                        ++distroCount;
                    }
                    addDiagnostic(L"Detected WSL distributions via direct registry fallback.");
                }
            }

            if (distroCount == 0) {
                bool isLocalTarget = target.node.empty() || target.node == L"." || _wcsicmp(target.node.c_str(), L"localhost") == 0;
                if (isLocalTarget && target.transport == TransportMode::DcomRpc) {
                    std::wstring wslOutput;
                    int wslExit = -1;
                    std::wstring wslPath;
                    if (ResolveSystemExecutablePath(L"wsl.exe", wslPath) &&
                        RunCommandCapture(QuoteCommandLineArgument(wslPath) + L" -l -q", wslOutput, wslExit) && wslExit == 0) {
                        std::vector<std::pair<std::wstring, std::wstring>> cliDistros;
                        if (ParseWslListOutput(wslOutput, cliDistros, L"Detected via wsl.exe")) {
                            for (const auto& entry : cliDistros) {
                                addRow(L"WSL Distro", entry.first, entry.second);
                                ++distroCount;
                            }
                            addDiagnostic(L"Detected WSL distributions via wsl.exe -l -q fallback.");
                        }
                    }
                }
            }

            if (distroCount > 0) {
                wslPlatformState = L"Enabled";
                updatePlatformRow(L"Windows Subsystem for Linux", L"Enabled");
                updatePlatformRow(L"WSL (Aggregated)", L"Enabled");
            }

            if (distroCount == 0) {
                addRow(L"WSL Distro", L"Installed Distributions", L"None found (or different user profile context)");
                addDiagnostic(L"No WSL distributions were discovered from registry or CLI fallback paths.");
            }

            pRegClass->Release();
        } else {
            addRow(L"WSL Distro", L"Installed Distributions", L"Unavailable (StdRegProv access failed)");
        }

        pRegSvc->Release();
    } else {
        addRow(L"WSL Distro", L"Installed Distributions", L"Unavailable");
        addDiagnostic(L"Root\\DEFAULT registry access was unavailable for WSL distro discovery.");
    }

    for (const auto& note : wslDiagnostics) {
        addRow(L"WSL Diagnostics", L"Probe", note);
    }

    if (table.rows.empty()) {
        g_lastNoData = true;
        if (g_outputOptions.format == OutputFormat::Table) {
            std::wcout << L"  No data available.\n" << std::endl;
        }
        return false;
    }

    table.Print();
    return true;
}
bool ExecuteWMISwitchStateQuery(const WmiTargetConfig& target, const std::wstring& sectionTitle, const std::wstring& wqlQuery, const std::vector<std::pair<std::wstring, std::wstring>>& switches, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    g_lastNoData = false;

    IWbemServices* pSvc = NULL;
    if (!ConnectWmiServices(target, L"ROOT\\CIMV2", securityConfig, wmiLocale, &pSvc)) {
        return false;
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    HRESULT hr = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(wqlQuery.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );

    if (FAILED(hr)) {
        std::wcerr << L"[!] WMI Query execution failed: " << DescribeWmiHResult(hr) << std::endl;
        pSvc->Release();
        return false;
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

    if (g_outputOptions.format == OutputFormat::Table) {
        std::wcout << L"\n" << sectionTitle << L":" << std::endl;
    }
    if (uReturn == 0 || FAILED(hr)) {
        g_lastNoData = true;
        std::wcout << L"  No data available.\n" << std::endl;
        pEnumerator->Release();
        pSvc->Release();
        return false;
    }

    Table table;
    table.headers = { L"Switch", L"State" };

    for (const auto& sw : switches) {
        VARIANT vtProp;
        VariantInit(&vtProp);

        hr = pclsObj->Get(sw.first.c_str(), 0, &vtProp, 0, 0);
        if (SUCCEEDED(hr)) {
            table.rows.push_back({ sw.second, FormatSwitchState(VariantToString(vtProp)) });
            VariantClear(&vtProp);
        } else {
            table.rows.push_back({ sw.second, L"N/A" });
        }
    }

    table.Print();

    pclsObj->Release();
    pEnumerator->Release();
    pSvc->Release();
    return true;
}
bool ExecuteGetDiskPreset(const WmiTargetConfig& target, const std::wstring& whereClause, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    return ExecuteWMIQuery(
        target,
        BuildWqlWithOptionalWhere(
            L"SELECT Index, Model, InterfaceType, MediaType, Size, SerialNumber, Status FROM Win32_DiskDrive",
            whereClause
        ),
        { L"Index", L"Model", L"InterfaceType", L"MediaType", L"Size", L"SerialNumber", L"Status" },
        true,
        wmiLocale,
        { L"Disk", L"Model", L"Interface", L"Media Type", L"Size", L"Serial Number", L"Status" },
        securityConfig
    );
}
bool ExecuteWMIQuery(const WmiTargetConfig& target, const std::wstring& wqlQuery, const std::vector<std::wstring>& properties, bool autoByteFormat, const std::wstring& wmiLocale, const std::vector<std::wstring>& displayHeaders, const WmiSecurityConfig& securityConfig) {
    if (target.transport == TransportMode::Ssh) {
        std::wstring out;
        std::wstring error;
        bool noData = false;
        if (!SshPowerShellBackend::ExecuteQuery(target, wqlQuery, properties, g_outputOptions, out, noData, error)) {
            std::wcerr << L"[!] " << error << std::endl;
            return false;
        }

        if (noData) {
            g_lastNoData = true;
            return true;
        }

        std::wcout << out;
        if (!out.empty() && out.back() != L'\n') std::wcout << L"\n";
        return true;
    }

    if (target.transport == TransportMode::WinRm) {
        const IWinRmBackend& backend = GetWinRmBackend(target.winrmBackend);
        std::wstring out;
        std::wstring error;
        bool noData = false;
        if (!backend.ExecuteQuery(target, wqlQuery, properties, g_outputOptions, out, noData, error)) {
            std::wcerr << L"[!] " << error << std::endl;
            return false;
        }

        if (noData) {
            g_lastNoData = true;
            return true;
        }

        std::wcout << out;
        if (!out.empty() && out.back() != L'\n') std::wcout << L"\n";
        return true;
    }

    g_lastNoData = false;

    IWbemServices* pSvc = NULL;
    if (!ConnectWmiServices(target, target.wmiNamespace, securityConfig, wmiLocale, &pSvc)) {
        return false;
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    HRESULT hr = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(wqlQuery.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );

    if (FAILED(hr)) {
        std::wcerr << L"[!] WMI Query execution failed: " << DescribeWmiHResult(hr) << std::endl;
        pSvc->Release();
        return false;
    }

    if (g_outputOptions.format == OutputFormat::Json) {
        std::vector<std::wstring> jsonHeaders;
        if (!displayHeaders.empty() && displayHeaders.size() == properties.size()) {
            jsonHeaders = displayHeaders;
        } else {
            jsonHeaders = properties;
        }

        std::wstring jsonOut;
        jsonOut.reserve(4096);
        jsonOut.push_back(L'[');

        IWbemClassObject* pclsObj = NULL;
        ULONG uReturn = 0;
        bool firstRow = true;

        while (pEnumerator) {
            hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
            if (FAILED(hr)) {
                std::wcerr << L"[!] WMI result enumeration failed: " << DescribeWmiHResult(hr) << std::endl;
                pEnumerator->Release();
                pSvc->Release();
                return false;
            }
            if (uReturn == 0) break;

            if (!firstRow) jsonOut.push_back(L',');
            firstRow = false;

            jsonOut.push_back(L'{');
            for (size_t c = 0; c < properties.size(); ++c) {
                if (c > 0) jsonOut.push_back(L',');

                std::wstring key = EscapeJson(jsonHeaders[c]);
                jsonOut += L"\"";
                jsonOut += key;
                jsonOut += L"\":";

                VARIANT vtProp;
                VariantInit(&vtProp);
                hr = pclsObj->Get(properties[c].c_str(), 0, &vtProp, 0, 0);

                if (SUCCEEDED(hr)) {
                    if (properties[c] == L"InstallDate" || properties[c] == L"ReleaseDate") {
                        std::wstring val = FormatWmiDateTime(VariantToString(vtProp));
                        jsonOut += L"\"";
                        jsonOut += EscapeJson(val);
                        jsonOut += L"\"";
                    } else if (properties[c] == L"LicenseStatus") {
                        std::wstring val = MapLicenseStatusCode(VariantToString(vtProp));
                        jsonOut += L"\"";
                        jsonOut += EscapeJson(val);
                        jsonOut += L"\"";
                    } else {
                        jsonOut += VariantToJson(vtProp);
                    }
                } else {
                    jsonOut += L"null";
                }

                VariantClear(&vtProp);
            }
            jsonOut.push_back(L'}');

            pclsObj->Release();
        }

        if (firstRow) {
            g_lastNoData = true;
        }

        jsonOut += L"]\n";
        std::wcout << jsonOut;

        pEnumerator->Release();
        pSvc->Release();
        return true;
    }

    Table table;
    if (!displayHeaders.empty() && displayHeaders.size() == properties.size()) {
        table.headers = displayHeaders;
    } else {
        table.headers = properties;
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    while (pEnumerator) {
        hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
        if (FAILED(hr)) {
            std::wcerr << L"[!] WMI result enumeration failed: " << DescribeWmiHResult(hr) << std::endl;
            pEnumerator->Release();
            pSvc->Release();
            return false;
        }
        if (0 == uReturn) break;

        std::vector<std::wstring> row;
        row.reserve(properties.size());
        for (const auto& prop : properties) {
            VARIANT vtProp;
            VariantInit(&vtProp);
            hr = pclsObj->Get(prop.c_str(), 0, &vtProp, 0, 0);

            if (SUCCEEDED(hr)) {
                std::wstring val = VariantToString(vtProp);

                if (prop == L"InstallDate" || prop == L"ReleaseDate") {
                    val = FormatWmiDateTime(val);
                }

                if (prop == L"LicenseStatus") {
                    val = MapLicenseStatusCode(val);
                }
                
                if (autoByteFormat && (prop == L"Size" || prop == L"FreeSpace" || prop == L"Capacity" || prop == L"WorkingSetSize")) {
                    unsigned long long bytes = 0;
                    if (TryParseUnsignedLongLong(val, bytes)) {
                        val = FormatBytes(bytes);
                    }
                }
                
                row.push_back(val);
                VariantClear(&vtProp);
            } else {
                row.push_back(L"N/A");
            }
        }
        table.rows.push_back(std::move(row));
        pclsObj->Release();
    }

    if (table.rows.empty()) {
        g_lastNoData = true;
    }

    table.Print();

    pEnumerator->Release();
    pSvc->Release();
    return true;
}
bool ExecuteWmiMethodCall(const WmiTargetConfig& target, const std::wstring& classNameOrPath, const std::wstring& methodName, const std::map<std::wstring, std::wstring>& inputArgs, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    if (target.transport == TransportMode::Ssh) {
        std::wstring out;
        std::wstring error;
        if (!SshPowerShellBackend::ExecuteMethodCall(target, classNameOrPath, methodName, inputArgs, out, error)) {
            std::wcerr << L"[!] " << error << std::endl;
            return false;
        }

        std::wcout << L"\nMethod Execution Successful.";
        if (!out.empty()) {
            std::wstring trimmed = out;
            while (!trimmed.empty() && (trimmed.back() == L'\n' || trimmed.back() == L'\r')) trimmed.pop_back();
            if (!trimmed.empty()) std::wcout << L" " << trimmed;
        }
        std::wcout << L"\n" << std::endl;
        return true;
    }

    if (target.transport == TransportMode::WinRm) {
        const IWinRmBackend& backend = GetWinRmBackend(target.winrmBackend);
        std::wstring out;
        std::wstring error;
        if (!backend.ExecuteMethodCall(target, classNameOrPath, methodName, inputArgs, out, error)) {
            std::wcerr << L"[!] " << error << std::endl;
            return false;
        }

        std::wcout << L"\nMethod Execution Successful.";
        if (!out.empty()) {
            std::wstring trimmed = out;
            while (!trimmed.empty() && (trimmed.back() == L'\n' || trimmed.back() == L'\r')) trimmed.pop_back();
            if (!trimmed.empty()) std::wcout << L" " << trimmed;
        }
        std::wcout << L"\n" << std::endl;
        return true;
    }

    IWbemServices* pSvc = NULL;
    if (!ConnectWmiServices(target, target.wmiNamespace, securityConfig, wmiLocale, &pSvc)) {
        return false;
    }

    IWbemClassObject* pClass = NULL;
    HRESULT hr = pSvc->GetObject(_bstr_t(classNameOrPath.c_str()), 0, NULL, &pClass, NULL);
    if (FAILED(hr)) {
        std::wcerr << L"[!] Failed to get target object/class for method execution." << std::endl;
        pSvc->Release();
        return false;
    }

    IWbemClassObject* pInParamsDef = NULL;
    hr = pClass->GetMethod(methodName.c_str(), 0, &pInParamsDef, NULL);
    if (FAILED(hr)) {
        std::wcerr << L"[!] Method signature lookup failed for: " << methodName << std::endl;
        pClass->Release();
        pSvc->Release();
        return false;
    }

    IWbemClassObject* pInClass = NULL;
    if (pInParamsDef) {
        hr = pInParamsDef->SpawnInstance(0, &pInClass);
        if (FAILED(hr) || pInClass == NULL) {
            std::wcerr << L"[!] Failed to create method input parameter instance." << std::endl;
            pInParamsDef->Release();
            pClass->Release();
            pSvc->Release();
            return false;
        }

        auto isIntegerLiteral = [](const std::wstring& value) -> bool {
            if (value.empty()) return false;

            size_t start = 0;
            if (value[0] == L'+' || value[0] == L'-') {
                if (value.size() == 1) return false;
                start = 1;
            }

            for (size_t i = start; i < value.size(); ++i) {
                if (!iswdigit(value[i])) return false;
            }
            return true;
        };

        auto mapCimToVariantType = [](CIMTYPE baseType, VARTYPE& vtOut) -> bool {
            switch (baseType) {
            case CIM_SINT8: vtOut = VT_I1; return true;
            case CIM_UINT8: vtOut = VT_UI1; return true;
            case CIM_SINT16: vtOut = VT_I2; return true;
            case CIM_UINT16: vtOut = VT_UI2; return true;
            case CIM_SINT32: vtOut = VT_I4; return true;
            case CIM_UINT32: vtOut = VT_UI4; return true;
            case CIM_SINT64: vtOut = VT_I8; return true;
            case CIM_UINT64: vtOut = VT_UI8; return true;
            case CIM_REAL32: vtOut = VT_R4; return true;
            case CIM_REAL64: vtOut = VT_R8; return true;
            case CIM_BOOLEAN: vtOut = VT_BOOL; return true;
            case CIM_STRING:
            case CIM_DATETIME:
            case CIM_REFERENCE:
            case CIM_CHAR16:
            case CIM_OBJECT:
                vtOut = VT_BSTR;
                return true;
            default:
                return false;
            }
        };

        auto parseSignedInteger = [](const std::wstring& value, long long minimum, long long maximum, long long& parsedOut) -> bool {
            try {
                size_t processed = 0;
                long long parsed = std::stoll(value, &processed, 10);
                if (processed != value.size() || parsed < minimum || parsed > maximum) return false;
                parsedOut = parsed;
                return true;
            } catch (...) {
                return false;
            }
        };
        auto parseUnsignedInteger = [](const std::wstring& value, unsigned long long maximum, unsigned long long& parsedOut) -> bool {
            if (value.empty() || value.front() == L'-') return false;
            try {
                size_t processed = 0;
                unsigned long long parsed = std::stoull(value, &processed, 10);
                if (processed != value.size() || parsed > maximum) return false;
                parsedOut = parsed;
                return true;
            } catch (...) {
                return false;
            }
        };
        auto parseReal32 = [](const std::wstring& value, FLOAT& parsedOut) -> bool {
            try {
                size_t processed = 0;
                FLOAT parsed = std::stof(value, &processed);
                if (processed != value.size() || !std::isfinite(parsed)) return false;
                parsedOut = parsed;
                return true;
            } catch (...) {
                return false;
            }
        };
        auto parseReal64 = [](const std::wstring& value, DOUBLE& parsedOut) -> bool {
            try {
                size_t processed = 0;
                DOUBLE parsed = std::stod(value, &processed);
                if (processed != value.size() || !std::isfinite(parsed)) return false;
                parsedOut = parsed;
                return true;
            } catch (...) {
                return false;
            }
        };

        auto tryBuildScalarVariant = [&](CIMTYPE baseType, const std::wstring& rawValue, VARIANT& vtOut) -> bool {
            try {
                switch (baseType) {
                case CIM_SINT8: {
                    long long parsed = 0;
                    if (!parseSignedInteger(rawValue, std::numeric_limits<CHAR>::min(), std::numeric_limits<CHAR>::max(), parsed)) return false;
                    V_VT(&vtOut) = VT_I1;
                    V_I1(&vtOut) = static_cast<CHAR>(parsed);
                    return true;
                }
                case CIM_UINT8: {
                    unsigned long long parsed = 0;
                    if (!parseUnsignedInteger(rawValue, std::numeric_limits<BYTE>::max(), parsed)) return false;
                    V_VT(&vtOut) = VT_UI1;
                    V_UI1(&vtOut) = static_cast<BYTE>(parsed);
                    return true;
                }
                case CIM_SINT16: {
                    long long parsed = 0;
                    if (!parseSignedInteger(rawValue, std::numeric_limits<SHORT>::min(), std::numeric_limits<SHORT>::max(), parsed)) return false;
                    V_VT(&vtOut) = VT_I2;
                    V_I2(&vtOut) = static_cast<SHORT>(parsed);
                    return true;
                }
                case CIM_UINT16: {
                    unsigned long long parsed = 0;
                    if (!parseUnsignedInteger(rawValue, std::numeric_limits<USHORT>::max(), parsed)) return false;
                    V_VT(&vtOut) = VT_UI2;
                    V_UI2(&vtOut) = static_cast<USHORT>(parsed);
                    return true;
                }
                case CIM_SINT32: {
                    long long parsed = 0;
                    if (!parseSignedInteger(rawValue, std::numeric_limits<LONG>::min(), std::numeric_limits<LONG>::max(), parsed)) return false;
                    V_VT(&vtOut) = VT_I4;
                    V_I4(&vtOut) = static_cast<LONG>(parsed);
                    return true;
                }
                case CIM_UINT32: {
                    unsigned long long parsed = 0;
                    if (!parseUnsignedInteger(rawValue, std::numeric_limits<ULONG>::max(), parsed)) return false;
                    V_VT(&vtOut) = VT_UI4;
                    V_UI4(&vtOut) = static_cast<ULONG>(parsed);
                    return true;
                }
                case CIM_SINT64: {
                    long long parsed = 0;
                    if (!parseSignedInteger(rawValue, std::numeric_limits<LONGLONG>::min(), std::numeric_limits<LONGLONG>::max(), parsed)) return false;
                    V_VT(&vtOut) = VT_I8;
                    V_I8(&vtOut) = static_cast<LONGLONG>(parsed);
                    return true;
                }
                case CIM_UINT64: {
                    unsigned long long parsed = 0;
                    if (!parseUnsignedInteger(rawValue, std::numeric_limits<ULONGLONG>::max(), parsed)) return false;
                    V_VT(&vtOut) = VT_UI8;
                    V_UI8(&vtOut) = static_cast<ULONGLONG>(parsed);
                    return true;
                }
                case CIM_REAL32: {
                    FLOAT parsed = 0;
                    if (!parseReal32(rawValue, parsed)) return false;
                    V_VT(&vtOut) = VT_R4;
                    V_R4(&vtOut) = parsed;
                    return true;
                }
                case CIM_REAL64: {
                    DOUBLE parsed = 0;
                    if (!parseReal64(rawValue, parsed)) return false;
                    V_VT(&vtOut) = VT_R8;
                    V_R8(&vtOut) = parsed;
                    return true;
                }
                case CIM_BOOLEAN: {
                    std::wstring lowered = rawValue;
                    std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
                    if (lowered == L"true" || lowered == L"1") {
                        V_VT(&vtOut) = VT_BOOL;
                        V_BOOL(&vtOut) = VARIANT_TRUE;
                        return true;
                    }
                    if (lowered == L"false" || lowered == L"0") {
                        V_VT(&vtOut) = VT_BOOL;
                        V_BOOL(&vtOut) = VARIANT_FALSE;
                        return true;
                    }
                    return false;
                }
                case CIM_STRING:
                case CIM_DATETIME:
                case CIM_REFERENCE:
                case CIM_CHAR16:
                case CIM_OBJECT:
                    V_VT(&vtOut) = VT_BSTR;
                    V_BSTR(&vtOut) = SysAllocString(rawValue.c_str());
                    return V_BSTR(&vtOut) != NULL;
                default:
                    return false;
                }
            } catch (...) {
                return false;
            }
        };

        auto tryBuildArrayVariant = [&](CIMTYPE baseType, const std::wstring& rawValue, VARIANT& vtOut) -> bool {
            VARTYPE elementVt = VT_EMPTY;
            if (!mapCimToVariantType(baseType, elementVt)) {
                return false;
            }

            std::vector<std::wstring> items = SplitAndTrimCommaList(rawValue);
            if (items.empty()) {
                return false;
            }

            SAFEARRAYBOUND bound;
            bound.lLbound = 0;
            bound.cElements = static_cast<ULONG>(items.size());
            SAFEARRAY* psa = SafeArrayCreate(elementVt, 1, &bound);
            if (!psa) {
                return false;
            }

            bool ok = true;
            for (LONG i = 0; i < static_cast<LONG>(items.size()); ++i) {
                const std::wstring& item = items[i];
                HRESULT putHr = E_FAIL;

                try {
                    switch (elementVt) {
                    case VT_I1: {
                        long long parsed = 0;
                        if (!parseSignedInteger(item, std::numeric_limits<CHAR>::min(), std::numeric_limits<CHAR>::max(), parsed)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        CHAR v = static_cast<CHAR>(parsed);
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_UI1: {
                        unsigned long long parsed = 0;
                        if (!parseUnsignedInteger(item, std::numeric_limits<BYTE>::max(), parsed)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        BYTE v = static_cast<BYTE>(parsed);
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_I2: {
                        long long parsed = 0;
                        if (!parseSignedInteger(item, std::numeric_limits<SHORT>::min(), std::numeric_limits<SHORT>::max(), parsed)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        SHORT v = static_cast<SHORT>(parsed);
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_UI2: {
                        unsigned long long parsed = 0;
                        if (!parseUnsignedInteger(item, std::numeric_limits<USHORT>::max(), parsed)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        USHORT v = static_cast<USHORT>(parsed);
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_I4: {
                        long long parsed = 0;
                        if (!parseSignedInteger(item, std::numeric_limits<LONG>::min(), std::numeric_limits<LONG>::max(), parsed)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        LONG v = static_cast<LONG>(parsed);
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_UI4: {
                        unsigned long long parsed = 0;
                        if (!parseUnsignedInteger(item, std::numeric_limits<ULONG>::max(), parsed)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        ULONG v = static_cast<ULONG>(parsed);
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_I8: {
                        long long parsed = 0;
                        if (!parseSignedInteger(item, std::numeric_limits<LONGLONG>::min(), std::numeric_limits<LONGLONG>::max(), parsed)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        LONGLONG v = static_cast<LONGLONG>(parsed);
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_UI8: {
                        unsigned long long parsed = 0;
                        if (!parseUnsignedInteger(item, std::numeric_limits<ULONGLONG>::max(), parsed)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        ULONGLONG v = static_cast<ULONGLONG>(parsed);
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_R4: {
                        FLOAT v = 0;
                        if (!parseReal32(item, v)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_R8: {
                        DOUBLE v = 0;
                        if (!parseReal64(item, v)) {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_BOOL: {
                        std::wstring lowered = item;
                        std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
                        VARIANT_BOOL v;
                        if (lowered == L"true" || lowered == L"1") v = VARIANT_TRUE;
                        else if (lowered == L"false" || lowered == L"0") v = VARIANT_FALSE;
                        else {
                            putHr = E_INVALIDARG;
                            break;
                        }
                        putHr = SafeArrayPutElement(psa, &i, &v);
                        break;
                    }
                    case VT_BSTR: {
                        BSTR b = SysAllocString(item.c_str());
                        if (!b) {
                            putHr = E_OUTOFMEMORY;
                            break;
                        }
                        putHr = SafeArrayPutElement(psa, &i, b);
                        SysFreeString(b);
                        break;
                    }
                    default:
                        putHr = E_NOTIMPL;
                        break;
                    }
                } catch (...) {
                    putHr = E_INVALIDARG;
                }

                if (FAILED(putHr)) {
                    ok = false;
                    break;
                }
            }

            if (!ok) {
                SafeArrayDestroy(psa);
                return false;
            }

            V_VT(&vtOut) = static_cast<VARTYPE>(VT_ARRAY | elementVt);
            V_ARRAY(&vtOut) = psa;
            return true;
        };

        auto putTypedInputValue = [&](const std::wstring& name, const std::wstring& rawValue) -> bool {
            CIMTYPE cimType = 0;
            VARIANT vtSchema;
            VariantInit(&vtSchema);
            HRESULT schemaHr = pInParamsDef->Get(name.c_str(), 0, &vtSchema, &cimType, NULL);
            VariantClear(&vtSchema);

            VARIANT vtVal;
            VariantInit(&vtVal);
            bool assigned = false;
            bool hasSchemaType = SUCCEEDED(schemaHr);

            if (hasSchemaType) {
                CIMTYPE baseType = static_cast<CIMTYPE>(cimType & ~CIM_FLAG_ARRAY);
                bool isArray = (cimType & CIM_FLAG_ARRAY) != 0;
                assigned = isArray
                    ? tryBuildArrayVariant(baseType, rawValue, vtVal)
                    : tryBuildScalarVariant(baseType, rawValue, vtVal);

                if (!assigned) {
                    // Schema exists but conversion failed; avoid sending mismatched VARTYPE.
                    return false;
                }
            }

            // Fallback when schema is unavailable: best-effort integer or BSTR.
            if (!hasSchemaType && isIntegerLiteral(rawValue)) {
                try {
                    long long v = std::stoll(rawValue);
                    if (v >= static_cast<long long>(LONG_MIN) && v <= static_cast<long long>(LONG_MAX)) {
                        V_VT(&vtVal) = VT_I4;
                        V_I4(&vtVal) = static_cast<LONG>(v);
                    } else {
                        V_VT(&vtVal) = VT_I8;
                        V_I8(&vtVal) = static_cast<LONGLONG>(v);
                    }
                    assigned = true;
                } catch (...) {
                    assigned = false;
                }
            }

            if (!assigned) {
                V_VT(&vtVal) = VT_BSTR;
                V_BSTR(&vtVal) = SysAllocString(rawValue.c_str());
            }

            HRESULT putHr = pInClass->Put(name.c_str(), 0, &vtVal, 0);
            VariantClear(&vtVal);
            return SUCCEEDED(putHr);
        };

        for (const auto& kv : inputArgs) {
            if (!putTypedInputValue(kv.first, kv.second)) {
                std::wcerr << L"[!] Failed to set method input parameter: " << kv.first << std::endl;
                pInClass->Release();
                pInParamsDef->Release();
                pClass->Release();
                pSvc->Release();
                return false;
            }
        }

        pInParamsDef->Release();
    }

    IWbemClassObject* pOutParams = NULL;
    hr = pSvc->ExecMethod(_bstr_t(classNameOrPath.c_str()), _bstr_t(methodName.c_str()), 0, NULL, pInClass, &pOutParams, NULL);

    if (pInClass) pInClass->Release();
    pClass->Release();

    if (SUCCEEDED(hr) && pOutParams) {
        VARIANT vtRet;
        VariantInit(&vtRet);
        if (SUCCEEDED(pOutParams->Get(L"ReturnValue", 0, &vtRet, NULL, NULL))) {
            std::wcout << L"\nMethod Execution Successful. ReturnValue = " << VariantToString(vtRet) << L"\n" << std::endl;
        } else {
            std::wcout << L"\nMethod Executed Successfully.\n" << std::endl;
        }
        VariantClear(&vtRet);
        pOutParams->Release();
        pSvc->Release();
        return true;
    }

    std::wcerr << L"[!] Method execution failed: " << DescribeWmiHResult(hr) << std::endl;
    pSvc->Release();
    return false;
}
bool ExecuteWmiSetProperty(const WmiTargetConfig& target, const std::wstring& objectPath, const std::wstring& propName, const std::wstring& propValue, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    if (target.transport == TransportMode::WinRm) {
        const IWinRmBackend& backend = GetWinRmBackend(target.winrmBackend);
        std::wstring out;
        std::wstring error;
        if (!backend.ExecuteSetProperty(target, objectPath, propName, propValue, out, error)) {
            std::wcerr << L"[!] " << error << std::endl;
            return false;
        }

        if (!out.empty()) {
            std::wcout << out;
            if (out.back() != L'\n') std::wcout << L"\n";
        } else {
            std::wcout << L"\nProperty updated successfully: " << propName << L" = " << propValue << L"\n" << std::endl;
        }
        return true;
    }

    IWbemServices* pSvc = NULL;
    if (!ConnectWmiServices(target, target.wmiNamespace, securityConfig, wmiLocale, &pSvc)) {
        return false;
    }

    IWbemClassObject* pInst = NULL;
    HRESULT hr = pSvc->GetObject(_bstr_t(objectPath.c_str()), 0, NULL, &pInst, NULL);
    if (FAILED(hr)) {
        std::wcerr << L"[!] Instance not found for SET operation." << std::endl;
        pSvc->Release();
        return false;
    }

    VARIANT vtVal;
    VariantInit(&vtVal);
    V_VT(&vtVal) = VT_BSTR;
    V_BSTR(&vtVal) = SysAllocString(propValue.c_str());

    hr = pInst->Put(propName.c_str(), 0, &vtVal, 0);
    VariantClear(&vtVal);

    if (SUCCEEDED(hr)) {
        hr = pSvc->PutInstance(pInst, WBEM_FLAG_UPDATE_ONLY, NULL, NULL);
    }

    pInst->Release();
    pSvc->Release();

    if (SUCCEEDED(hr)) {
        std::wcout << L"\nProperty updated successfully: " << propName << L" = " << propValue << L"\n" << std::endl;
        return true;
    }

    std::wcerr << L"[!] Failed to update instance property: " << DescribeWmiHResult(hr) << std::endl;
    return false;
}
bool ExecuteWmiDelete(const WmiTargetConfig& target, const std::wstring& objectPath, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    if (target.transport == TransportMode::WinRm) {
        const IWinRmBackend& backend = GetWinRmBackend(target.winrmBackend);
        std::wstring out;
        std::wstring error;
        if (!backend.ExecuteDeleteInstance(target, objectPath, out, error)) {
            std::wcerr << L"[!] " << error << std::endl;
            return false;
        }

        if (!out.empty()) {
            std::wcout << out;
            if (out.back() != L'\n') std::wcout << L"\n";
        } else {
            std::wcout << L"\nInstance deleted successfully: " << objectPath << L"\n" << std::endl;
        }
        return true;
    }

    IWbemServices* pSvc = NULL;
    if (!ConnectWmiServices(target, target.wmiNamespace, securityConfig, wmiLocale, &pSvc)) {
        return false;
    }

    HRESULT hr = pSvc->DeleteInstance(_bstr_t(objectPath.c_str()), 0, NULL, NULL);
    pSvc->Release();

    if (SUCCEEDED(hr)) {
        std::wcout << L"\nInstance deleted successfully: " << objectPath << L"\n" << std::endl;
        return true;
    }

    std::wcerr << L"[!] Failed to delete instance: " << DescribeWmiHResult(hr) << std::endl;
    return false;
}
