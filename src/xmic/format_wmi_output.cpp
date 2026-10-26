#include "xmic_common.hpp"

const IWinRmBackend& GetWinRmBackend(WinRmBackendMode mode) {
    static WinRmPowerShellBackend powershellBackend;
    static WinRmNativeBackendStub nativeStubBackend;
    return (mode == WinRmBackendMode::Native)
        ? static_cast<const IWinRmBackend&>(nativeStubBackend)
        : static_cast<const IWinRmBackend&>(powershellBackend);
}
std::vector<std::wstring> SplitAndTrimCommaList(const std::wstring& input) {
    return SplitCommaTrimmed(input);
}
std::wstring TrimMatchingQuotes(const std::wstring& input) {
    if (input.size() >= 2) {
        wchar_t first = input.front();
        wchar_t last = input.back();
        if ((first == L'"' && last == L'"') || (first == L'\'' && last == L'\'')) {
            return input.substr(1, input.size() - 2);
        }
    }
    return input;
}
bool ParseNamedCallArguments(int startIndex, int argc, wchar_t* argv[], std::map<std::wstring, std::wstring>& argsOut, std::wstring& errorOut) {
    argsOut.clear();
    std::wstring currentKey;

    for (int i = startIndex; i < argc; ++i) {
        std::wstring token = argv[i];
        size_t eqPos = token.find(L'=');

        if (eqPos != std::wstring::npos && eqPos > 0) {
            std::wstring key = token.substr(0, eqPos);
            std::wstring value = TrimMatchingQuotes(token.substr(eqPos + 1));
            argsOut[key] = value;
            currentKey = key;
            continue;
        }

        if (!currentKey.empty()) {
            argsOut[currentKey] += L" " + TrimMatchingQuotes(token);
            continue;
        }

        errorOut = L"Unexpected call argument fragment: " + token + L" (expected Name=Value)";
        return false;
    }

    return true;
}
std::wstring FormatSwitchState(const std::wstring& value) {
    if (value == L"True") return L"Enabled";
    if (value == L"False") return L"Disabled";
    return value;
}
std::wstring GetProcessElevationStateString() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return L"Unknown";
    }

    TOKEN_ELEVATION elevation = {};
    DWORD returnLen = 0;
    BOOL ok = GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &returnLen);
    CloseHandle(hToken);

    if (!ok) {
        return L"Unknown";
    }

    return elevation.TokenIsElevated ? L"Elevated" : L"Standard";
}
std::wstring GetCurrentUserSidString() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        return L"Unavailable";
    }

    DWORD sizeNeeded = 0;
    GetTokenInformation(hToken, TokenUser, NULL, 0, &sizeNeeded);
    if (sizeNeeded == 0) {
        CloseHandle(hToken);
        return L"Unavailable";
    }

    std::vector<unsigned char> buffer(sizeNeeded);
    TOKEN_USER* pTokenUser = reinterpret_cast<TOKEN_USER*>(buffer.data());
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, sizeNeeded, &sizeNeeded)) {
        CloseHandle(hToken);
        return L"Unavailable";
    }

    LPWSTR sidStr = NULL;
    if (!ConvertSidToStringSidW(pTokenUser->User.Sid, &sidStr)) {
        CloseHandle(hToken);
        return L"Unavailable";
    }

    std::wstring sid = sidStr;
    LocalFree(sidStr);
    CloseHandle(hToken);
    return sid;
}
bool TryParseWmiSecurity(const std::wstring& input, WmiSecurityConfig& config) {
    std::wstring value = input;
    std::transform(value.begin(), value.end(), value.begin(), towlower);

    if (value == L"default") { config.authnLevel = RPC_C_AUTHN_LEVEL_DEFAULT; return true; }
    if (value == L"connect") { config.authnLevel = RPC_C_AUTHN_LEVEL_CONNECT; return true; }
    if (value == L"call") { config.authnLevel = RPC_C_AUTHN_LEVEL_CALL; return true; }
    if (value == L"pkt") { config.authnLevel = RPC_C_AUTHN_LEVEL_PKT; return true; }
    if (value == L"pktprivacy") { config.authnLevel = RPC_C_AUTHN_LEVEL_PKT_PRIVACY; return true; }

    return false;
}
bool TryParseOutputFormat(const std::wstring& input, OutputFormat& format) {
    std::wstring value = input;
    std::transform(value.begin(), value.end(), value.begin(), towlower);

    if (value == L"table") { format = OutputFormat::Table; return true; }
    if (value == L"csv") { format = OutputFormat::Csv; return true; }
    if (value == L"json") { format = OutputFormat::Json; return true; }

    return false;
}
std::wstring MapLicenseStatusCode(const std::wstring& rawValue) {
    if (rawValue == L"0") return L"Unlicensed";
    if (rawValue == L"1") return L"Licensed";
    if (rawValue == L"2") return L"OOB Grace";
    if (rawValue == L"3") return L"OOT Grace";
    if (rawValue == L"4") return L"Non-Genuine Grace";
    if (rawValue == L"5") return L"Notification";
    if (rawValue == L"6") return L"Extended Grace";
    return rawValue;
}
std::wstring MapOptionalFeatureInstallState(const std::wstring& rawValue) {
    if (rawValue == L"1") return L"Enabled";
    if (rawValue == L"2") return L"Disabled";
    if (rawValue == L"3") return L"Absent";
    return rawValue;
}
std::wstring MapHyperVEnabledState(const std::wstring& rawValue) {
    if (rawValue == L"2") return L"Enabled";
    if (rawValue == L"3") return L"Disabled";
    if (rawValue == L"32768") return L"Paused";
    if (rawValue == L"32769") return L"Suspended";
    if (rawValue == L"32770") return L"Starting";
    if (rawValue == L"32771") return L"Snapshotting";
    if (rawValue == L"32773") return L"Saving";
    if (rawValue == L"32774") return L"Stopping";
    return rawValue;
}
