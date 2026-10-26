#include "xmic_common.hpp"

bool ExecuteThreadsInfoQuery(const WmiTargetConfig& target, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    g_lastNoData = false;

    IWbemServices* pSvc = NULL;
    if (!ConnectWmiServices(target, L"ROOT\\CIMV2", securityConfig, wmiLocale, &pSvc)) {
        return false;
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    HRESULT hr = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(L"SELECT Threads FROM Win32_PerfFormattedData_PerfOS_System"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );

    if (FAILED(hr)) {
        pSvc->Release();
        return false;
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
    if (FAILED(hr) || uReturn == 0) {
        g_lastNoData = true;
        if (g_outputOptions.format == OutputFormat::Table) {
            std::wcout << L"  No data available.\n" << std::endl;
        }
        pEnumerator->Release();
        pSvc->Release();
        return false;
    }

    VARIANT vtThreads;
    VariantInit(&vtThreads);
    std::wstring threads = L"N/A";
    if (SUCCEEDED(pclsObj->Get(L"Threads", 0, &vtThreads, 0, 0))) {
        threads = VariantToString(vtThreads);
    }
    VariantClear(&vtThreads);

    pclsObj->Release();
    pEnumerator->Release();
    pSvc->Release();

    Table table;
    if (g_outputOptions.format == OutputFormat::Table) {
        table.headers = { L"Property", L"Value" };
        table.rows.push_back({ L"Running Threads", threads });
    } else {
        table.headers = { L"Running Threads" };
        table.rows.push_back({ threads });
    }

    table.Print();
    return true;
}
std::wstring HResultToHex(HRESULT hr) {
    wchar_t buffer[11] = {};
    _snwprintf_s(buffer, _countof(buffer), _TRUNCATE, L"0x%08lX", static_cast<unsigned long>(hr));
    return std::wstring(buffer);
}
std::wstring GetSystemMessageForHResult(HRESULT hr) {
    LPWSTR raw = NULL;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    DWORD len = FormatMessageW(flags, NULL, static_cast<DWORD>(hr), langId, reinterpret_cast<LPWSTR>(&raw), 0, NULL);
    if (len == 0 || raw == NULL) {
        return L"";
    }

    std::wstring text(raw, len);
    LocalFree(raw);

    while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ' || text.back() == L'\t')) {
        text.pop_back();
    }
    return text;
}
std::wstring DescribeWmiHResult(HRESULT hr) {
    std::wstring symbol = L"HRESULT";
    std::wstring hint = L"Review namespace/class/property names, permissions, and transport/authentication settings.";

    switch (hr) {
    case WBEM_E_ACCESS_DENIED:
        symbol = L"WBEM_E_ACCESS_DENIED";
        hint = L"Access denied. Run elevated if needed, verify account rights, namespace ACLs, and UAC remote restrictions.";
        break;
    case WBEM_E_NOT_FOUND:
        symbol = L"WBEM_E_NOT_FOUND";
        hint = L"Requested object was not found. Verify class/object path and where filter values.";
        break;
    case WBEM_E_PROVIDER_FAILURE:
        symbol = L"WBEM_E_PROVIDER_FAILURE";
        hint = L"WMI provider failed. Retry, check provider/service health, and inspect Event Viewer WMI-Activity logs.";
        break;
    case WBEM_E_INVALID_NAMESPACE:
        symbol = L"WBEM_E_INVALID_NAMESPACE";
        hint = L"Invalid namespace. Verify --namespace value and that the namespace exists on target.";
        break;
    case WBEM_E_INVALID_CLASS:
        symbol = L"WBEM_E_INVALID_CLASS";
        hint = L"Invalid class. Confirm class name exists in the selected namespace on target.";
        break;
    case WBEM_E_INVALID_QUERY:
        symbol = L"WBEM_E_INVALID_QUERY";
        hint = L"Invalid WQL syntax. Validate SELECT/WHERE clauses and quoted values.";
        break;
    case WBEM_E_INVALID_QUERY_TYPE:
        symbol = L"WBEM_E_INVALID_QUERY_TYPE";
        hint = L"Unsupported query language. Use WQL queries for WMI operations.";
        break;
    case WBEM_E_INVALID_PARAMETER:
        symbol = L"WBEM_E_INVALID_PARAMETER";
        hint = L"Invalid parameter passed to provider/API. Check command arguments and property/value types.";
        break;
    case WBEM_E_OUT_OF_MEMORY:
        symbol = L"WBEM_E_OUT_OF_MEMORY";
        hint = L"Provider reported memory pressure. Reduce query scope/volume and retry.";
        break;
    case WBEM_E_TRANSPORT_FAILURE:
        symbol = L"WBEM_E_TRANSPORT_FAILURE";
        hint = L"Transport failure. Verify RPC/WinRM connectivity, firewall, DNS, and endpoint availability.";
        break;
    case WBEM_E_CALL_CANCELLED:
        symbol = L"WBEM_E_CALL_CANCELLED";
        hint = L"Operation was cancelled or timed out. Retry and reduce query scope if needed.";
        break;
    case E_ACCESSDENIED:
        symbol = L"E_ACCESSDENIED";
        hint = L"Access denied. Verify credentials, privileges, and remote management policy.";
        break;
    case HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE):
        symbol = L"RPC_S_SERVER_UNAVAILABLE";
        hint = L"RPC server unavailable. Verify target hostname, network reachability, firewall, and RPC service.";
        break;
    default:
        break;
    }

    std::wstring systemText = GetSystemMessageForHResult(hr);
    std::wstring out = HResultToHex(hr) + L" (" + symbol + L")";
    if (!systemText.empty()) {
        out += L": " + systemText;
    }
    if (!hint.empty()) {
        out += L". " + hint;
    }
    return out;
}
bool ProbeWmiSecurityLevel(const WmiTargetConfig& target, const WmiSecurityConfig& securityConfig, const std::wstring& wmiLocale, std::wstring& details) {
    IWbemServices* pSvc = NULL;
    if (!ConnectWmiServices(target, L"ROOT\\CIMV2", securityConfig, wmiLocale, &pSvc)) {
        details = L"ConnectServer/ProxyBlanket failed";
        return false;
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    HRESULT hr = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(L"SELECT Caption FROM Win32_OperatingSystem"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator
    );

    if (FAILED(hr)) {
        details = L"WMI probe query failed (" + DescribeWmiHResult(hr) + L")";
        pSvc->Release();
        return false;
    }

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
    bool available = SUCCEEDED(hr) && uReturn > 0;

    if (available) {
        details = L"Available";
        pclsObj->Release();
    } else {
        details = L"Probe returned no data (" + DescribeWmiHResult(hr) + L")";
    }

    pEnumerator->Release();
    pSvc->Release();

    return available;
}
void PrintWmiSecurityAvailability(const WmiTargetConfig& target, const std::wstring& wmiLocale) {
    std::vector<std::wstring> levels = { L"default", L"connect", L"call", L"pkt", L"pktprivacy" };

    Table table;
    table.headers = { L"Security Level", L"Status", L"Details" };

    for (const auto& level : levels) {
        WmiSecurityConfig cfg;
        TryParseWmiSecurity(level, cfg);

        std::wstring details;
        bool ok = ProbeWmiSecurityLevel(target, cfg, wmiLocale, details);
        table.rows.push_back({ level, ok ? L"Available" : L"Unavailable", details });
    }

    if (g_outputOptions.format == OutputFormat::Table) {
        std::wcout << L"\nWMI Security Capability Check:" << std::endl;
    }
    table.Print();
}
std::wstring ReadRegistryStringValue(IWbemServices* pSvc, IWbemClassObject* pRegClass, const std::wstring& subKey, const std::wstring& valueName) {
    const unsigned long HKEY_LOCAL_MACHINE_HIVE = 0x80000002;
    return ReadRegistryStringValueWithHive(pSvc, pRegClass, HKEY_LOCAL_MACHINE_HIVE, subKey, valueName);
}
std::wstring ReadRegistryStringValueWithHive(IWbemServices* pSvc, IWbemClassObject* pRegClass, unsigned long hive, const std::wstring& subKey, const std::wstring& valueName) {
    IWbemClassObject* pInDef = NULL;
    HRESULT hr = pRegClass->GetMethod(L"GetStringValue", 0, &pInDef, NULL);
    if (FAILED(hr) || pInDef == NULL) return L"";

    IWbemClassObject* pInParams = NULL;
    hr = pInDef->SpawnInstance(0, &pInParams);
    pInDef->Release();
    if (FAILED(hr) || pInParams == NULL) return L"";

    VARIANT vt;
    VariantInit(&vt);

    V_VT(&vt) = VT_UI4;
    V_UI4(&vt) = hive;
    pInParams->Put(L"hDefKey", 0, &vt, 0);
    VariantClear(&vt);

    V_VT(&vt) = VT_BSTR;
    V_BSTR(&vt) = SysAllocString(subKey.c_str());
    pInParams->Put(L"sSubKeyName", 0, &vt, 0);
    VariantClear(&vt);

    V_VT(&vt) = VT_BSTR;
    V_BSTR(&vt) = SysAllocString(valueName.c_str());
    pInParams->Put(L"sValueName", 0, &vt, 0);
    VariantClear(&vt);

    IWbemClassObject* pOutParams = NULL;
    hr = pSvc->ExecMethod(_bstr_t(L"StdRegProv"), _bstr_t(L"GetStringValue"), 0, NULL, pInParams, &pOutParams, NULL);
    pInParams->Release();
    if (FAILED(hr) || pOutParams == NULL) return L"";

    VARIANT vtOut;
    VariantInit(&vtOut);
    std::wstring result;
    hr = pOutParams->Get(L"sValue", 0, &vtOut, NULL, NULL);
    if (SUCCEEDED(hr) && V_VT(&vtOut) == VT_BSTR && V_BSTR(&vtOut) != NULL) {
        result = V_BSTR(&vtOut);
    }
    VariantClear(&vtOut);
    pOutParams->Release();
    return result;
}
bool ReadRegistryDwordValueWithHive(IWbemServices* pSvc, IWbemClassObject* pRegClass, unsigned long hive, const std::wstring& subKey, const std::wstring& valueName, unsigned long& valueOut) {
    valueOut = 0;

    IWbemClassObject* pInDef = NULL;
    HRESULT hr = pRegClass->GetMethod(L"GetDWORDValue", 0, &pInDef, NULL);
    if (FAILED(hr) || pInDef == NULL) return false;

    IWbemClassObject* pInParams = NULL;
    hr = pInDef->SpawnInstance(0, &pInParams);
    pInDef->Release();
    if (FAILED(hr) || pInParams == NULL) return false;

    VARIANT vt;
    VariantInit(&vt);

    V_VT(&vt) = VT_UI4;
    V_UI4(&vt) = hive;
    pInParams->Put(L"hDefKey", 0, &vt, 0);
    VariantClear(&vt);

    V_VT(&vt) = VT_BSTR;
    V_BSTR(&vt) = SysAllocString(subKey.c_str());
    pInParams->Put(L"sSubKeyName", 0, &vt, 0);
    VariantClear(&vt);

    V_VT(&vt) = VT_BSTR;
    V_BSTR(&vt) = SysAllocString(valueName.c_str());
    pInParams->Put(L"sValueName", 0, &vt, 0);
    VariantClear(&vt);

    IWbemClassObject* pOutParams = NULL;
    hr = pSvc->ExecMethod(_bstr_t(L"StdRegProv"), _bstr_t(L"GetDWORDValue"), 0, NULL, pInParams, &pOutParams, NULL);
    pInParams->Release();
    if (FAILED(hr) || pOutParams == NULL) return false;

    VARIANT vtOut;
    VariantInit(&vtOut);
    bool success = false;
    hr = pOutParams->Get(L"uValue", 0, &vtOut, NULL, NULL);
    if (SUCCEEDED(hr) && (V_VT(&vtOut) == VT_UI4 || V_VT(&vtOut) == VT_I4)) {
        valueOut = (V_VT(&vtOut) == VT_UI4) ? V_UI4(&vtOut) : static_cast<unsigned long>(V_I4(&vtOut));
        success = true;
    }
    VariantClear(&vtOut);
    pOutParams->Release();
    return success;
}
bool EnumRegistrySubKeysWithHive(IWbemServices* pSvc, IWbemClassObject* pRegClass, unsigned long hive, const std::wstring& subKey, std::vector<std::wstring>& namesOut) {
    namesOut.clear();

    IWbemClassObject* pInDef = NULL;
    HRESULT hr = pRegClass->GetMethod(L"EnumKey", 0, &pInDef, NULL);
    if (FAILED(hr) || pInDef == NULL) return false;

    IWbemClassObject* pInParams = NULL;
    hr = pInDef->SpawnInstance(0, &pInParams);
    pInDef->Release();
    if (FAILED(hr) || pInParams == NULL) return false;

    VARIANT vt;
    VariantInit(&vt);

    V_VT(&vt) = VT_UI4;
    V_UI4(&vt) = hive;
    pInParams->Put(L"hDefKey", 0, &vt, 0);
    VariantClear(&vt);

    V_VT(&vt) = VT_BSTR;
    V_BSTR(&vt) = SysAllocString(subKey.c_str());
    pInParams->Put(L"sSubKeyName", 0, &vt, 0);
    VariantClear(&vt);

    IWbemClassObject* pOutParams = NULL;
    hr = pSvc->ExecMethod(_bstr_t(L"StdRegProv"), _bstr_t(L"EnumKey"), 0, NULL, pInParams, &pOutParams, NULL);
    pInParams->Release();
    if (FAILED(hr) || pOutParams == NULL) return false;

    VARIANT vtNames;
    VariantInit(&vtNames);
    hr = pOutParams->Get(L"sNames", 0, &vtNames, NULL, NULL);
    if (FAILED(hr) || !(V_VT(&vtNames) & VT_ARRAY) || V_ARRAY(&vtNames) == NULL) {
        VariantClear(&vtNames);
        pOutParams->Release();
        return false;
    }

    long lBound = 0;
    long uBound = -1;
    SafeArrayGetLBound(V_ARRAY(&vtNames), 1, &lBound);
    SafeArrayGetUBound(V_ARRAY(&vtNames), 1, &uBound);

    for (long i = lBound; i <= uBound; ++i) {
        BSTR keyNameBstr = NULL;
        hr = SafeArrayGetElement(V_ARRAY(&vtNames), &i, &keyNameBstr);
        if (SUCCEEDED(hr) && keyNameBstr != NULL) {
            namesOut.push_back(std::wstring(keyNameBstr));
            SysFreeString(keyNameBstr);
        }
    }

    VariantClear(&vtNames);
    pOutParams->Release();
    return true;
}
bool CollectInstalledAppsByArchitecture(const WmiTargetConfig& target, bool showWin32Apps, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig, std::vector<InstalledAppRecord>& recordsOut) {
    recordsOut.clear();

    IWbemServices* pSvc = NULL;
    if (!ConnectWmiServices(target, L"ROOT\\DEFAULT", securityConfig, wmiLocale, &pSvc)) {
        return false;
    }

    IWbemClassObject* pRegClass = NULL;
    HRESULT hr = pSvc->GetObject(_bstr_t(L"StdRegProv"), 0, NULL, &pRegClass, NULL);
    if (FAILED(hr) || pRegClass == NULL) {
        pSvc->Release();
        return false;
    }

    const std::wstring uninstallPath = showWin32Apps
        ? L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
        : L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

    const unsigned long HKEY_LOCAL_MACHINE_HIVE = 0x80000002;
    std::vector<std::wstring> keyNames;
    if (!EnumRegistrySubKeysWithHive(pSvc, pRegClass, HKEY_LOCAL_MACHINE_HIVE, uninstallPath, keyNames)) {
        pRegClass->Release();
        pSvc->Release();
        return false;
    }

    std::vector<InstalledAppRecord> records;
    records.reserve(keyNames.size());
    for (const auto& keyName : keyNames) {

        std::wstring fullSubKey = uninstallPath + L"\\" + keyName;
        std::wstring appName = ReadRegistryStringValue(pSvc, pRegClass, fullSubKey, L"DisplayName");
        if (appName.empty()) {
            continue;
        }

        InstalledAppRecord rec;
        rec.name = appName;
        rec.version = ReadRegistryStringValue(pSvc, pRegClass, fullSubKey, L"DisplayVersion");
        rec.publisher = ReadRegistryStringValue(pSvc, pRegClass, fullSubKey, L"Publisher");
        if (rec.version.empty()) rec.version = L"N/A";
        if (rec.publisher.empty()) rec.publisher = L"N/A";
        records.push_back(std::move(rec));
    }

    pRegClass->Release();
    pSvc->Release();

    std::sort(records.begin(), records.end(), [](const InstalledAppRecord& a, const InstalledAppRecord& b) {
        return a.name < b.name;
    });

    recordsOut = std::move(records);
    return true;
}
bool PrintInstalledAppsByArchitecture(const WmiTargetConfig& target, bool showWin32Apps, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    g_lastNoData = false;
    std::vector<InstalledAppRecord> records;
    if (!CollectInstalledAppsByArchitecture(target, showWin32Apps, wmiLocale, securityConfig, records)) {
        return false;
    }

    Table table;
    table.headers = { L"Name", L"Version", L"Publisher" };
    for (const auto& rec : records) {
        table.rows.push_back({ rec.name, rec.version, rec.publisher });
    }

    if (g_outputOptions.format == OutputFormat::Table) {
        std::wcout << L"\n"
                   << (showWin32Apps ? L"Installed Win32 Applications:" : L"Installed Win64 Applications:")
                   << std::endl;
    }

    if (table.rows.empty()) {
        g_lastNoData = true;
        std::wcout << L"  No data available.\n" << std::endl;
        return true;
    }

    table.Print();
    return true;
}
bool PrintInstalledAppsAggregated(const WmiTargetConfig& target, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    g_lastNoData = false;

    std::vector<InstalledAppRecord> records64;
    std::vector<InstalledAppRecord> records32;

    bool ok64 = CollectInstalledAppsByArchitecture(target, false, wmiLocale, securityConfig, records64);
    bool ok32 = CollectInstalledAppsByArchitecture(target, true, wmiLocale, securityConfig, records32);

    if (!ok64 && !ok32) {
        return false;
    }

    auto toLower = [](std::wstring value) {
        std::transform(value.begin(), value.end(), value.begin(), towlower);
        return value;
    };

    std::set<std::wstring> seen;
    std::vector<InstalledAppRecord> merged;
    merged.reserve(records64.size() + records32.size());

    auto appendUnique = [&](const std::vector<InstalledAppRecord>& src) {
        for (const auto& rec : src) {
            std::wstring key = toLower(rec.name) + L"|" + toLower(rec.version) + L"|" + toLower(rec.publisher);
            if (seen.insert(key).second) {
                merged.push_back(rec);
            }
        }
    };

    appendUnique(records64);
    appendUnique(records32);

    std::sort(merged.begin(), merged.end(), [](const InstalledAppRecord& a, const InstalledAppRecord& b) {
        return a.name < b.name;
    });

    Table table;
    table.headers = { L"Name", L"Version", L"Publisher" };
    for (const auto& rec : merged) {
        table.rows.push_back({ rec.name, rec.version, rec.publisher });
    }

    if (g_outputOptions.format == OutputFormat::Table) {
        std::wcout << L"\nInstalled Applications (Win64 + Win32):" << std::endl;
    }

    if (table.rows.empty()) {
        g_lastNoData = true;
        std::wcout << L"  No data available.\n" << std::endl;
        return true;
    }

    table.Print();
    return true;
}
bool PrintInstalledAppsViaPowerShellTransport(const WmiTargetConfig& target, int architectureMode, std::wstring& errorOut) {
    errorOut.clear();
    g_lastNoData = false;

    if (!target.user.empty() || !target.password.empty() || !target.domain.empty()) {
        errorOut = L"apps over non-DCOM transport currently requires current logon token (no explicit credentials).";
        return false;
    }

    auto buildScript = [&](bool forSsh) -> std::wstring {
        std::wstring script;
        script += L"$ErrorActionPreference='Stop';";
        script += L"[uint32]$hklm=2147483650;";

        if (!forSsh && !target.node.empty() && target.node != L"." && _wcsicmp(target.node.c_str(), L"localhost") != 0) {
            script += L"$cimOpts=@{ComputerName='" + EscapePowerShellSingleQuoted(target.node) + L"'};";
        } else {
            script += L"$cimOpts=@{};";
        }

        script += L"function Get-AppRecords([string]$subKey,[hashtable]$opts){";
        script += L"$r=Invoke-CimMethod @opts -Namespace 'root/default' -ClassName 'StdRegProv' -MethodName 'EnumKey' -Arguments @{hDefKey=[uint32]$hklm;sSubKeyName=$subKey};";
        script += L"if($null -eq $r -or $null -eq $r.sNames){return @()};";
        script += L"$rows=@();";
        script += L"foreach($key in $r.sNames){";
        script += L"$full=$subKey+'\\'+$key;";
        script += L"$name=(Invoke-CimMethod @opts -Namespace 'root/default' -ClassName 'StdRegProv' -MethodName 'GetStringValue' -Arguments @{hDefKey=[uint32]$hklm;sSubKeyName=$full;sValueName='DisplayName'}).sValue;";
        script += L"if([string]::IsNullOrWhiteSpace($name)){continue};";
        script += L"$ver=(Invoke-CimMethod @opts -Namespace 'root/default' -ClassName 'StdRegProv' -MethodName 'GetStringValue' -Arguments @{hDefKey=[uint32]$hklm;sSubKeyName=$full;sValueName='DisplayVersion'}).sValue;";
        script += L"$pub=(Invoke-CimMethod @opts -Namespace 'root/default' -ClassName 'StdRegProv' -MethodName 'GetStringValue' -Arguments @{hDefKey=[uint32]$hklm;sSubKeyName=$full;sValueName='Publisher'}).sValue;";
        script += L"if([string]::IsNullOrWhiteSpace($ver)){$ver='N/A'}; if([string]::IsNullOrWhiteSpace($pub)){$pub='N/A'};";
        script += L"$rows += [PSCustomObject]@{Name=$name;Version=$ver;Publisher=$pub};";
        script += L"};";
        script += L"return $rows;";
        script += L"};";

        script += L"$records=@();";
        if (architectureMode == 0 || architectureMode == 64) {
            script += L"$records += Get-AppRecords 'SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall' $cimOpts;";
        }
        if (architectureMode == 0 || architectureMode == 32) {
            script += L"$records += Get-AppRecords 'SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall' $cimOpts;";
        }

        if (architectureMode == 0) {
            script += L"$records = $records | Group-Object Name,Version,Publisher | ForEach-Object { $_.Group[0] };";
        }

        script += L"$records = $records | Sort-Object Name,Version,Publisher;";

        if (g_outputOptions.format == OutputFormat::Json) {
            script += L"$records | ConvertTo-Json -Depth 4 -Compress";
        } else if (g_outputOptions.format == OutputFormat::Csv) {
            script += L"$csv=$records | ConvertTo-Csv -NoTypeInformation;";
            if (g_outputOptions.noHeader) {
                script += L"if($csv.Count -gt 1){$csv=$csv[1..($csv.Count-1)]}else{$csv=@()};";
            }
            script += L"$csv";
        } else {
            script += L"$records | Format-Table -Property Name,Version,Publisher -AutoSize";
        }

        return script;
    };

    std::wstring output;
    int code = -1;
    bool ok = false;
    if (target.transport == TransportMode::Ssh) {
        ok = RunSshPowerShellScriptCapture(target, buildScript(true), output, code);
    } else if (target.transport == TransportMode::WinRm) {
        ok = RunPowerShellCommandCapture(buildScript(false), output, code);
    } else {
        errorOut = L"Invalid transport for PrintInstalledAppsViaPowerShellTransport.";
        return false;
    }

    if (!ok || code != 0) {
        errorOut = (target.transport == TransportMode::Ssh)
            ? L"SSH PowerShell apps backend failed."
            : L"WinRM PowerShell apps backend failed.";
        std::wstring details = TrimWhitespace(output);
        if (!details.empty()) {
            errorOut += L" Details: " + details;
        }
        return false;
    }

    std::wstring trimmed = TrimWhitespace(output);
    if (trimmed.empty()) {
        g_lastNoData = true;
        std::wcout << L"  No data available.\n" << std::endl;
        return true;
    }

    std::wcout << output;
    if (output.back() != L'\n') std::wcout << L"\n";
    return true;
}
