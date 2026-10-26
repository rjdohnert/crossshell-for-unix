#include "xmic_common.hpp"

std::wstring DescribeWslProbeError(WslProbeError error) {
    switch (error) {
    case WslProbeError::None: return L"Success";
    case WslProbeError::NoDistrosFound: return L"No distros found";
    case WslProbeError::UnsupportedTransport: return L"Unsupported transport for local WSL probing";
    case WslProbeError::RemoteTargetSkipped: return L"Remote target skipped";
    case WslProbeError::AccessDenied: return L"Access denied";
    case WslProbeError::RegistryUnavailable: return L"Registry path unavailable";
    case WslProbeError::WslNotInstalled: return L"WSL CLI not available";
    case WslProbeError::PowerShellFailed: return L"PowerShell fallback failed";
    case WslProbeError::UnsupportedHost: return L"Unsupported host environment";
    case WslProbeError::Unknown:
    default: return L"Unknown error";
    }
}
void AppendWslDiagnostic(WslDetectionResult& result, const std::wstring& message) {
    if (!message.empty()) {
        result.diagnostics.push_back(message);
    }
}
void AppendWslError(WslDetectionResult& result, WslProbeError error, const std::wstring& detail) {
    if (result.status == WslProbeError::None || result.status == WslProbeError::NoDistrosFound) {
        result.status = error;
    }
    if (!detail.empty()) {
        result.errors.push_back(detail);
    }
}
std::wstring FormatWslDetectionSummary(const WslDetectionResult& result) {
    std::wstring summary = DescribeWslProbeError(result.status);
    if (!result.diagnostics.empty()) {
        summary += L"; " + result.diagnostics.front();
    }
    return summary;
}
bool ReadRegistryStringValueDirect(HKEY hive, const std::wstring& subKey, const std::wstring& valueName, std::wstring& valueOut) {
    valueOut.clear();

    HKEY key = nullptr;
    if (RegOpenKeyExW(hive, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD dataSize = 0;
    LONG rc = RegQueryValueExW(key, valueName.c_str(), nullptr, &type, nullptr, &dataSize);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(key);
        return false;
    }

    std::vector<wchar_t> buffer((dataSize / sizeof(wchar_t)) + 1);
    rc = RegQueryValueExW(key, valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &dataSize);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS) {
        return false;
    }

    valueOut = buffer.data();
    return true;
}
bool ReadRegistryDwordValueDirect(HKEY hive, const std::wstring& subKey, const std::wstring& valueName, DWORD& valueOut) {
    valueOut = 0;

    HKEY key = nullptr;
    if (RegOpenKeyExW(hive, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD dataSize = sizeof(DWORD);
    DWORD value = 0;
    LONG rc = RegQueryValueExW(key, valueName.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(&value), &dataSize);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || type != REG_DWORD) {
        return false;
    }

    valueOut = value;
    return true;
}
bool EnumerateWslDistrosFromRegistry(HKEY hive, const std::wstring& rootSubKey, std::vector<std::pair<std::wstring, std::wstring>>& distrosOut) {
    distrosOut.clear();

    HKEY key = nullptr;
    if (RegOpenKeyExW(hive, rootSubKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD subKeyCount = 0;
    DWORD maxNameLength = 0;
    LONG rc = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subKeyCount, &maxNameLength, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS) {
        return false;
    }

    if (subKeyCount == 0) {
        return false;
    }

    if (RegOpenKeyExW(hive, rootSubKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return false;
    }

    std::vector<wchar_t> nameBuffer(maxNameLength + 1);
    for (DWORD i = 0; i < subKeyCount; ++i) {
        DWORD nameLength = static_cast<DWORD>(nameBuffer.size());
        if (RegEnumKeyExW(key, i, nameBuffer.data(), &nameLength, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
            continue;
        }

        std::wstring subKey = rootSubKey + L"\\" + nameBuffer.data();
        std::wstring distroName;
        if (!ReadRegistryStringValueDirect(hive, subKey, L"DistributionName", distroName)) {
            continue;
        }

        std::wstring versionText = L"Version Unknown";
        DWORD version = 0;
        if (ReadRegistryDwordValueDirect(hive, subKey, L"Version", version)) {
            versionText = L"WSL" + std::to_wstring(version);
        }

        distrosOut.push_back({ distroName, versionText });
    }

    RegCloseKey(key);
    return !distrosOut.empty();
}
bool CollectWslDetectionResult(const WmiTargetConfig& target, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig, WslDetectionResult& result) {
    result = WslDetectionResult{};

    result.isLocalTarget = target.node.empty() || target.node == L"." || _wcsicmp(target.node.c_str(), L"localhost") == 0;

    if (target.transport != TransportMode::DcomRpc) {
        AppendWslDiagnostic(result, L"WSL probing is only supported for local DCOM targets.");
        AppendWslError(result, WslProbeError::UnsupportedTransport, L"Unsupported transport for WSL probing.");
        return false;
    }

    if (!result.isLocalTarget) {
        AppendWslDiagnostic(result, L"Skipping WSL probe on remote target because local registry and CLI fallback are unavailable.");
        AppendWslError(result, WslProbeError::RemoteTargetSkipped, L"Remote targets are not probed for WSL fallbacks.");
        return false;
    }

    IWbemServices* pRegSvc = NULL;
    if (!ConnectWmiServices(target, L"ROOT\\DEFAULT", securityConfig, wmiLocale, &pRegSvc)) {
        AppendWslDiagnostic(result, L"Unable to connect to ROOT\\DEFAULT for WSL registry probing.");
        AppendWslError(result, WslProbeError::RegistryUnavailable, L"WMI connection to ROOT\\DEFAULT failed.");
        return false;
    }

    result.attemptedRegistry = true;

    IWbemClassObject* pRegClass = NULL;
    HRESULT hr = pRegSvc->GetObject(_bstr_t(L"StdRegProv"), 0, NULL, &pRegClass, NULL);
    if (SUCCEEDED(hr) && pRegClass != NULL) {
        result.registryAccessible = true;
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

                result.distros.push_back({ distroName, value });
                ++distroCount;
            }
        }

        if (distroCount == 0) {
            std::vector<std::pair<std::wstring, std::wstring>> directDistros;
            bool directOk = EnumerateWslDistrosFromRegistry(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Lxss", directDistros);
            if (!directOk) {
                directOk = EnumerateWslDistrosFromRegistry(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Lxss", directDistros);
            }
            if (directOk) {
                result.distros.insert(result.distros.end(), directDistros.begin(), directDistros.end());
                AppendWslDiagnostic(result, L"Detected WSL distributions via direct registry fallback.");
            }
        }

        if (!result.distros.empty()) {
            result.foundDistros = true;
            result.status = WslProbeError::None;
        }

        pRegClass->Release();
    } else {
        AppendWslDiagnostic(result, L"StdRegProv access failed; falling back to direct registry and CLI probing.");
        AppendWslError(result, WslProbeError::AccessDenied, L"StdRegProv access failed.");
    }

    if (!result.foundDistros) {
        result.attemptedCli = true;
        std::wstring wslOutput;
        int wslExit = -1;
        std::wstring wslPath;
        bool cliOk = ResolveSystemExecutablePath(L"wsl.exe", wslPath) &&
            RunCommandCapture(QuoteCommandLineArgument(wslPath) + L" -l -q", wslOutput, wslExit);
        if (cliOk && wslExit == 0) {
            result.cliSucceeded = true;
            ParseWslListOutput(wslOutput, result.distros, L"Detected via wsl.exe");
            if (!result.distros.empty()) {
                result.foundDistros = true;
                AppendWslDiagnostic(result, L"Detected WSL distributions via wsl.exe -l -q fallback.");
            }
        } else {
            AppendWslError(result, WslProbeError::PowerShellFailed, L"WSL CLI fallback command did not succeed.");
            if (wslExit != 0) {
                AppendWslDiagnostic(result, L"wsl.exe returned a non-zero exit code during fallback probing.");
            }
        }
    }

    if (!result.foundDistros) {
        result.status = WslProbeError::NoDistrosFound;
        AppendWslDiagnostic(result, L"No WSL distributions were discovered from registry or CLI fallback paths.");
    }

    if (pRegSvc) {
        pRegSvc->Release();
    }

    if (!result.errors.empty()) {
        std::wclog << L"[WSL] " << FormatWslDetectionSummary(result) << std::endl;
    }

    return true;
}
bool ParseWslListOutput(const std::wstring& wslOutput, std::vector<std::pair<std::wstring, std::wstring>>& distrosOut, const std::wstring& sourceLabel) {
    size_t pos = 0;
    while (pos <= wslOutput.size()) {
        size_t lineEnd = wslOutput.find_first_of(L"\r\n", pos);
        if (lineEnd == std::wstring::npos) {
            lineEnd = wslOutput.size();
        }

        std::wstring trimmed = TrimWhitespace(wslOutput.substr(pos, lineEnd - pos));
        trimmed.erase(std::remove(trimmed.begin(), trimmed.end(), L'\0'), trimmed.end());
        trimmed = TrimWhitespace(trimmed);

        if (!trimmed.empty() && trimmed.size() > 1) {
            std::wstring lowered = trimmed;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
            if (lowered.find(L"for information") != 0 &&
                lowered.find(L"windows subsystem for linux has no installed distributions") != 0 &&
                lowered.find(L"https://aka.ms/") != 0 &&
                lowered.find(L"the operation completed successfully") != 0) {
                distrosOut.push_back({ trimmed, sourceLabel });
            }
        }

        if (lineEnd == wslOutput.size()) {
            break;
        }

        pos = lineEnd;
        while (pos < wslOutput.size() && (wslOutput[pos] == L'\r' || wslOutput[pos] == L'\n')) {
            ++pos;
        }
    }

    return !distrosOut.empty();
}
