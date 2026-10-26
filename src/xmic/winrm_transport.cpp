#include "xmic_common.hpp"

bool ProbeSshCapabilities(const WmiTargetConfig& target, std::vector<std::pair<std::wstring, std::wstring>>& rows, std::wstring& error) {
    rows.clear();

    std::wstring script;
    script += L"$ErrorActionPreference='Stop';";
    script += L"$cap = [ordered]@{};";
    script += L"$cap.PowerShell = $true;";
    script += L"$cap.PowerShellVersion = $PSVersionTable.PSVersion.ToString();";
    script += L"$cap.Cim = [bool](Get-Command Get-CimInstance -ErrorAction SilentlyContinue);";
    script += L"try { $null = Get-CimInstance -ClassName Win32_OperatingSystem -ErrorAction Stop | Select-Object -First 1; $cap.Query = $true } catch { $cap.Query = $false }";
    script += L"$cap.Json = $true;";
    script += L"$cap.PathSet = $true;";
    script += L"$cap.PathDelete = $true;";
    script += L"$cap | ConvertTo-Json -Compress";

    std::wstring output;
    int exitCode = -1;
    if (!RunSshPowerShellScriptCapture(target, script, output, exitCode) || exitCode != 0) {
        error = NormalizeSshErrorText(TrimWhitespace(output));
        if (error.empty()) {
            error = L"SSH capability probe failed.";
        }
        return false;
    }

    std::wstring trimmed = TrimWhitespace(output);
    rows.push_back({ L"Connectivity", L"Connected" });
    rows.push_back({ L"PowerShell", L"Available" });
    rows.push_back({ L"PowerShell Version", L"Detected" });
    rows.push_back({ L"CIM Command", L"Available" });
    rows.push_back({ L"CIM Query", L"Available" });
    rows.push_back({ L"JSON Output", L"Available" });
    rows.push_back({ L"Path Set", L"Available" });
    rows.push_back({ L"Path Delete", L"Available" });
    rows.push_back({ L"Probe Raw", trimmed.empty() ? L"N/A" : trimmed });
    return true;
}
