#include "xmic_common.hpp"

bool ExecuteRamStatusSummary(const WmiTargetConfig& target, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig) {
    if (target.transport != TransportMode::DcomRpc) {
        return ExecuteWMIQuery(
            target,
            L"SELECT TotalVisibleMemorySize, FreePhysicalMemory FROM Win32_OperatingSystem",
            { L"TotalVisibleMemorySize", L"FreePhysicalMemory" },
            false,
            wmiLocale,
            { L"Total Visible Memory (KB)", L"Free Physical Memory (KB)" },
            securityConfig
        );
    }

    IWbemServices* pSvc = NULL;
    if (!ConnectWmiServices(target, L"ROOT\\CIMV2", securityConfig, wmiLocale, &pSvc)) {
        return false;
    }

    IEnumWbemClassObject* pEnumerator = NULL;
    HRESULT hr = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(L"SELECT TotalVisibleMemorySize, FreePhysicalMemory FROM Win32_OperatingSystem"),
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
        pEnumerator->Release();
        pSvc->Release();
        return false;
    }

    auto readProp = [&](const wchar_t* propName) -> std::wstring {
        VARIANT vtProp;
        VariantInit(&vtProp);
        std::wstring value = L"N/A";
        if (SUCCEEDED(pclsObj->Get(propName, 0, &vtProp, 0, 0))) {
            value = VariantToString(vtProp);
        }
        VariantClear(&vtProp);
        return value;
    };

    std::wstring totalKbStr = readProp(L"TotalVisibleMemorySize");
    std::wstring freeKbStr = readProp(L"FreePhysicalMemory");

    std::wstring ramLoad = L"N/A";
    std::wstring totalPhysical = L"N/A";
    std::wstring availablePhysical = L"N/A";

    unsigned long long totalKb = 0;
    unsigned long long freeKb = 0;
    if (TryParseUnsignedLongLong(totalKbStr, totalKb) && TryParseUnsignedLongLong(freeKbStr, freeKb)) {
        unsigned long long usedKb = (totalKb > freeKb) ? (totalKb - freeKb) : 0;

        if (totalKb > 0) {
            unsigned long long loadPct = (usedKb * 100ULL) / totalKb;
            ramLoad = std::to_wstring(loadPct) + L"%";
        }

        totalPhysical = FormatBytes(totalKb * 1024ULL);
        availablePhysical = FormatBytes(freeKb * 1024ULL);
    }

    if (g_outputOptions.format == OutputFormat::Table) {
        std::wcout << L"\n--- System Memory Summary ---" << std::endl;
        std::wcout << L"RAM Load:            " << ramLoad << std::endl;
        std::wcout << L"Total Physical:      " << totalPhysical << std::endl;
        std::wcout << L"Available Physical:  " << availablePhysical << std::endl;
    } else {
        Table table;
        table.headers = { L"RAM Load", L"Total Physical", L"Available Physical" };
        table.rows.push_back({ ramLoad, totalPhysical, availablePhysical });
        table.Print();
    }

    pclsObj->Release();
    pEnumerator->Release();
    pSvc->Release();
    return true;
}
