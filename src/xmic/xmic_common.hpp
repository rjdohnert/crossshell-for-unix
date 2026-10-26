#pragma once
/*
BSD 3-Clause License

Copyright (c) 2026, Roberto J. Dohnert
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
Single-File Index (Maintenance Map)

1) Core configuration and argument models
    - WmiSecurityConfig
    - WmiTargetConfig
    - OutputOptions

2) Local utility helpers
    - Console/input helpers (ReadHiddenConsoleLine, ConfigureConsoleOutputMode)
    - Formatting and parsing helpers (FormatBytes, VariantToJson, SplitCommaTrimmed)
    - Process execution helpers (RunProcessDirectCapture, RunCommandCapture)

3) Transport backends
    - SshPowerShellBackend
    - WinRmPowerShellBackend
    - WinRmNativeBackendStub

4) Query and operation execution
    - ExecuteWMIQuery
    - ExecuteWmiMethodCall
    - ExecuteWmiSetProperty
    - ExecuteWmiDelete

5) Preset and feature command handlers
    - ExecuteVirtualSwitchQuery
    - ExecuteThreadsInfoQuery
    - PrintInstalledAppsAggregated / PrintInstalledAppsByArchitecture

6) CLI/front-end flow
    - RunInteractiveShell
    - PrintHelp
    - wmain

Maintenance Checklist (Single-File Discipline)
1) If adding/changing a command:
    - Update command dispatch and alias handling in wmain.
    - Verify where-clause support behavior is explicit and consistent.
    - Update PrintHelp command and option text.
2) If adding/changing a global option:
    - Parse in global option scan.
    - Validate conflicts/requirements in one place.
    - Ensure script/shell/benchmark interactions are still valid.
3) If touching output formatting:
    - Verify table/csv/json parity for no-header and timing/benchmark modes.
4) Before merging:
    - Build, run at least one preset and one path query.
    - If performance-sensitive, run --benchmark-runs with --benchmark-quiet.
*/

#define _WIN32_DCOM
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <utility>
#include <set>
#include <map>
#include <cwctype>
#include <cerrno>
#include <cmath>
#include <limits>
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#include <mi.h>
#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "mi.lib")

// --- Helper Data Structures & Configs ---

struct WmiSecurityConfig {
    LONG authnLevel = RPC_C_AUTHN_LEVEL_DEFAULT;
    LONG impLevel = RPC_C_IMP_LEVEL_IMPERSONATE;
};

enum class TransportMode {
    DcomRpc,
    WinRm,
    Ssh
};

enum class WinRmBackendMode {
    PowerShell,
    Native
};

struct WmiTargetConfig {
    std::wstring node = L".";
    std::wstring user;
    std::wstring password;
    std::wstring domain;
    std::wstring sshKeyFile;
    std::wstring sshKeyPassphrase;
    std::wstring sshStrictHostKeyChecking;
    std::wstring sshKnownHostsFile;
    std::wstring sshProxyJump;
    int sshPort = 22;
    std::wstring wmiNamespace = L"ROOT\\CIMV2";
    TransportMode transport = TransportMode::DcomRpc;
    WinRmBackendMode winrmBackend = WinRmBackendMode::Native;
    bool sshPromptKeyPassphrase = false;
};

enum class OutputFormat {
    Table,
    Csv,
    Json
};

enum class ConsoleEncoding {
    Utf16,
    Utf8
};

struct OutputOptions {
    OutputFormat format = OutputFormat::Table;
    bool noHeader = false;
    bool verbose = false;
    std::wstring outputFile;
};

enum ExitCode {
    EXIT_OK = 0,
    EXIT_BAD_ARGS = 2,
    EXIT_UNKNOWN_COMMAND = 3,
    EXIT_WMI_FAILURE = 4,
    EXIT_NO_DATA = 5
};

extern OutputOptions g_outputOptions;
extern bool g_lastNoData;

constexpr size_t kMaxCapturedOutputChars = 8 * 1024 * 1024;
constexpr int kMaxScriptDepth = 8;
constexpr const wchar_t* kScriptDepthEnvVar = L"XMIC_SCRIPT_DEPTH";
constexpr const wchar_t* kScriptStackEnvVar = L"XMIC_SCRIPT_STACK";
constexpr const wchar_t* kToolVersion = L"2.2.0";
constexpr int kBenchmarkRunsMin = 1;
constexpr int kBenchmarkRunsMax = 10000;
constexpr const wchar_t* kBenchmarkRunsOption = L"--benchmark-runs";
constexpr const wchar_t* kBenchmarkQuietOption = L"--benchmark-quiet";
constexpr const wchar_t* kBenchmarkRunsMissingMessage = L"Missing value for --benchmark-runs.";
constexpr const wchar_t* kBenchmarkRunsInvalidMessage = L"Invalid --benchmark-runs value. Use an integer between 1 and 10000.";
constexpr const wchar_t* kBenchmarkInteractivePromptConflictMessage = L"--benchmark-runs cannot be used with interactive password prompts. Provide explicit credentials or run without benchmarking.";
constexpr const wchar_t* kBenchmarkQuietRequiresRunsMessage = L"--benchmark-quiet requires --benchmark-runs <N> where N > 1.";
constexpr const wchar_t* kBenchmarkMissingCommandMessage = L"--benchmark-runs requires a command to execute.";
constexpr const wchar_t* kBenchmarkBannerPrefix = L"[benchmark] Running '";
constexpr const wchar_t* kBenchmarkBannerSuffix = L"' for ";
constexpr const wchar_t* kBenchmarkIterationSuffix = L" iteration(s)...";
constexpr const wchar_t* kBenchmarkSummaryPrefix = L"[benchmark] Runs=";
constexpr const wchar_t* kBenchmarkSummaryMinLabel = L", Min=";
constexpr const wchar_t* kBenchmarkSummaryAvgLabel = L", Avg=";
constexpr const wchar_t* kBenchmarkSummaryMedLabel = L", Med=";
constexpr const wchar_t* kBenchmarkSummaryP95Label = L", P95=";
constexpr const wchar_t* kBenchmarkSummaryMaxLabel = L", Max=";
constexpr const wchar_t* kBenchmarkSummaryUnits = L" ms";

class StdoutRedirector;
class ScopedEnvironmentVariable;
class ScopedTempFile;
class IWinRmBackend;
class WinRmPowerShellBackend;
class SshPowerShellBackend;
class WinRmNativeBackendStub;
class ScopedMiOperation;
class ScopedMiSession;
class ScopedMiInstance;
struct Table;
struct InstalledAppRecord;
enum class WslProbeError;
struct WslDetectionResult;
struct PasswordScrubber;
extern OutputOptions g_outputOptions;
extern bool g_lastNoData;
bool TryParseSshStrictHostKeyCheckingMode(const std::wstring& input, std::wstring& normalizedMode);
bool TryParseTransportMode(const std::wstring& input, TransportMode& mode);
bool TryParseWinRmBackendMode(const std::wstring& input, WinRmBackendMode& mode);
bool TryParseConsoleEncoding(const std::wstring& input, ConsoleEncoding& encoding);
bool ConfigureConsoleOutputMode(ConsoleEncoding encoding);
bool ReadHiddenConsoleLine(const std::wstring& prompt, std::wstring& value, std::wstring& error);
double ElapsedMilliseconds(
    const std::chrono::steady_clock::time_point& start,
    const std::chrono::steady_clock::time_point& end
);
std::wstring DescribeWmiConnectionFailure(HRESULT hr);
bool ConnectWmiServices(
    const WmiTargetConfig& target,
    const std::wstring& defaultNamespace,
    const WmiSecurityConfig& securityConfig,
    const std::wstring& wmiLocale,
    IWbemServices** ppSvc,
    bool reportError = true
);
std::wstring FormatBytes(unsigned long long bytes);
bool TryParseUnsignedLongLong(const std::wstring& input, unsigned long long& valueOut);
std::wstring FormatJsonFloatingToken(double value);
std::wstring VariantToString(VARIANT& vtProp);
std::wstring FormatWmiDateTime(const std::wstring& wmiDateTime);
std::wstring EscapeJson(const std::wstring& input);
bool IsIntegerLiteral(const std::wstring& value);
std::wstring VariantToJson(VARIANT& vtProp);
std::wstring EscapeCsv(const std::wstring& input);
std::wstring EscapePowerShellSingleQuoted(const std::wstring& input);
bool RunProcessDirectCapture(const std::wstring& applicationPath, const std::wstring& commandLine, std::wstring& output, int& exitCode);
bool RunPowerShellCommandCapture(const std::wstring& script, std::wstring& output, int& exitCode);
bool RunCommandCapture(const std::wstring& command, std::wstring& output, int& exitCode);
bool ResolveExecutableOnPath(const std::wstring& executableName, std::wstring& resolvedPath);
bool ResolveSystemExecutablePath(const std::wstring& relativePath, std::wstring& resolvedPath);
std::wstring EncodePowerShellCommandBase64(const std::wstring& script);
std::wstring BuildSshTarget(const WmiTargetConfig& target);
std::wstring BuildSshControlPathTemplate();
std::wstring QuoteShellArg(const std::wstring& value);
std::wstring TrimWhitespace(const std::wstring& value);
std::vector<std::wstring> SplitCommaTrimmed(const std::wstring& input);
bool StartsWithCaseInsensitive(const std::wstring& value, const std::wstring& prefix);
std::wstring BuildWqlWithOptionalWhere(const std::wstring& baseQuery, const std::wstring& whereClause);
bool TryBuildInstanceSelectQueryFromObjectPath(const std::wstring& objectPath, std::wstring& queryOut);
bool TryGetEnvironmentVariableValue(const wchar_t* name, std::wstring& value);
bool CreateUniqueTempFile(const wchar_t* prefix, const wchar_t* extension, ScopedTempFile& tempFile);
bool WriteUtf8TextFile(ScopedTempFile& tempFile, const std::wstring& content);
bool CreateSshAskPassHelper(ScopedTempFile& helperFile);
bool RunSshCommandCaptureWithOptionalPassphrase(
    const WmiTargetConfig& target,
    const std::wstring& sshCommand,
    std::wstring& output,
    int& exitCode
);
std::wstring NormalizePathForComparison(const std::wstring& path);
bool ContainsPathInSemicolonList(const std::wstring& list, const std::wstring& candidate);
std::wstring AppendPathToSemicolonList(const std::wstring& list, const std::wstring& path);
bool TryParseScriptDepth(const std::wstring& value, int& depth);
std::wstring SanitizeScriptCommandForLog(const std::wstring& commandLine);
bool ExecuteScriptFile(
    const std::wstring& scriptPath,
    const std::wstring& executablePath,
    bool stopOnError,
    bool echoCommands,
    int currentScriptDepth,
    const std::wstring& inheritedScriptStack,
    int& exitCodeOut);
std::wstring BuildSshBaseCommand(const WmiTargetConfig& target);
bool RunSshPowerShellScriptCapture(const WmiTargetConfig& target, const std::wstring& script, std::wstring& output, int& exitCode);
bool RunSshConnectivityPreflight(const WmiTargetConfig& target, std::wstring& details);
std::wstring NormalizeSshErrorText(const std::wstring& rawText);
bool ProbeSshCapabilities(const WmiTargetConfig& target, std::vector<std::pair<std::wstring, std::wstring>>& rows, std::wstring& error);
const IWinRmBackend& GetWinRmBackend(WinRmBackendMode mode);
std::vector<std::wstring> SplitAndTrimCommaList(const std::wstring& input);
std::wstring TrimMatchingQuotes(const std::wstring& input);
bool ParseNamedCallArguments(int startIndex, int argc, wchar_t* argv[], std::map<std::wstring, std::wstring>& argsOut, std::wstring& errorOut);
std::wstring FormatSwitchState(const std::wstring& value);
std::wstring GetProcessElevationStateString();
std::wstring GetCurrentUserSidString();
bool TryParseWmiSecurity(const std::wstring& input, WmiSecurityConfig& config);
bool TryParseOutputFormat(const std::wstring& input, OutputFormat& format);
std::wstring MapLicenseStatusCode(const std::wstring& rawValue);
std::wstring MapOptionalFeatureInstallState(const std::wstring& rawValue);
std::wstring MapHyperVEnabledState(const std::wstring& rawValue);
bool ExecuteThreadsInfoQuery(const WmiTargetConfig& target, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig);
std::wstring HResultToHex(HRESULT hr);
std::wstring GetSystemMessageForHResult(HRESULT hr);
std::wstring DescribeWmiHResult(HRESULT hr);
bool ProbeWmiSecurityLevel(const WmiTargetConfig& target, const WmiSecurityConfig& securityConfig, const std::wstring& wmiLocale, std::wstring& details);
void PrintWmiSecurityAvailability(const WmiTargetConfig& target, const std::wstring& wmiLocale);
std::wstring ReadRegistryStringValue(IWbemServices* pSvc, IWbemClassObject* pRegClass, const std::wstring& subKey, const std::wstring& valueName);
std::wstring ReadRegistryStringValueWithHive(
    IWbemServices* pSvc,
    IWbemClassObject* pRegClass,
    unsigned long hive,
    const std::wstring& subKey,
    const std::wstring& valueName
);
bool ReadRegistryDwordValueWithHive(
    IWbemServices* pSvc,
    IWbemClassObject* pRegClass,
    unsigned long hive,
    const std::wstring& subKey,
    const std::wstring& valueName,
    unsigned long& valueOut
);
bool EnumRegistrySubKeysWithHive(
    IWbemServices* pSvc,
    IWbemClassObject* pRegClass,
    unsigned long hive,
    const std::wstring& subKey,
    std::vector<std::wstring>& namesOut
);
bool CollectInstalledAppsByArchitecture(
    const WmiTargetConfig& target,
    bool showWin32Apps,
    const std::wstring& wmiLocale,
    const WmiSecurityConfig& securityConfig,
    std::vector<InstalledAppRecord>& recordsOut
);
bool PrintInstalledAppsByArchitecture(const WmiTargetConfig& target, bool showWin32Apps, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig);
bool PrintInstalledAppsAggregated(const WmiTargetConfig& target, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig);
bool PrintInstalledAppsViaPowerShellTransport(const WmiTargetConfig& target, int architectureMode, std::wstring& errorOut);
std::wstring DescribeWslProbeError(WslProbeError error);
void AppendWslDiagnostic(WslDetectionResult& result, const std::wstring& message);
void AppendWslError(WslDetectionResult& result, WslProbeError error, const std::wstring& detail);
std::wstring FormatWslDetectionSummary(const WslDetectionResult& result);
bool ReadRegistryStringValueDirect(HKEY hive, const std::wstring& subKey, const std::wstring& valueName, std::wstring& valueOut);
bool ReadRegistryDwordValueDirect(HKEY hive, const std::wstring& subKey, const std::wstring& valueName, DWORD& valueOut);
bool EnumerateWslDistrosFromRegistry(HKEY hive, const std::wstring& rootSubKey, std::vector<std::pair<std::wstring, std::wstring>>& distrosOut);
bool CollectWslDetectionResult(
    const WmiTargetConfig& target,
    const std::wstring& wmiLocale,
    const WmiSecurityConfig& securityConfig,
    WslDetectionResult& result
);
bool ParseWslListOutput(const std::wstring& wslOutput, std::vector<std::pair<std::wstring, std::wstring>>& distrosOut, const std::wstring& sourceLabel);
bool ExecuteVirtualSwitchQuery(const WmiTargetConfig& target, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig);
bool ExecuteWMISwitchStateQuery(
    const WmiTargetConfig& target,
    const std::wstring& sectionTitle,
    const std::wstring& wqlQuery,
    const std::vector<std::pair<std::wstring, std::wstring>>& switches,
    const std::wstring& wmiLocale = L"",
    const WmiSecurityConfig& securityConfig = WmiSecurityConfig()
);
bool ExecuteGetDiskPreset(
    const WmiTargetConfig& target,
    const std::wstring& whereClause,
    const std::wstring& wmiLocale,
    const WmiSecurityConfig& securityConfig
);
bool ExecuteWMIQuery(
    const WmiTargetConfig& target,
    const std::wstring& wqlQuery,
    const std::vector<std::wstring>& properties,
    bool autoByteFormat = true,
    const std::wstring& wmiLocale = L"",
    const std::vector<std::wstring>& displayHeaders = {},
    const WmiSecurityConfig& securityConfig = WmiSecurityConfig()
);
bool ExecuteWmiMethodCall(
    const WmiTargetConfig& target,
    const std::wstring& classNameOrPath,
    const std::wstring& methodName,
    const std::map<std::wstring, std::wstring>& inputArgs,
    const std::wstring& wmiLocale,
    const WmiSecurityConfig& securityConfig
);
bool ExecuteWmiSetProperty(
    const WmiTargetConfig& target,
    const std::wstring& objectPath,
    const std::wstring& propName,
    const std::wstring& propValue,
    const std::wstring& wmiLocale,
    const WmiSecurityConfig& securityConfig
);
bool ExecuteWmiDelete(
    const WmiTargetConfig& target,
    const std::wstring& objectPath,
    const std::wstring& wmiLocale,
    const WmiSecurityConfig& securityConfig
);
bool ExecuteRamStatusSummary(const WmiTargetConfig& target, const std::wstring& wmiLocale, const WmiSecurityConfig& securityConfig);
bool ResolveExecutablePath(const std::wstring& candidate, std::wstring& resolvedPath);
std::wstring QuoteCommandLineArgument(const std::wstring& value);
std::wstring FormatSystemErrorMessage(DWORD errorCode);
std::vector<std::wstring> TokenizeShellInput(const std::wstring& input);
bool RunChildProcessAndWait(const std::wstring& executablePath, const std::vector<std::wstring>& args, std::wstring& errorOut, int& exitCode, bool suppressOutput = false);
bool TryParsePositiveInt(const std::wstring& input, int& valueOut);
bool TryParseBenchmarkRunsValue(int valueIndex, int argc, wchar_t* argv[], int& runsOut, std::wstring& errorOut);
bool ValidateBenchmarkOptions(
    int benchmarkRuns,
    bool benchmarkQuiet,
    bool interactivePasswordRequired,
    std::wstring& errorOut
);
int RunInteractiveShell(const std::wstring& executablePath);
void PrintHelp();
int RunWslDetectionSelfTests();
int wmain(int argc, wchar_t* argv[]);



// --- RAII Output Redirection ---

class StdoutRedirector {
private:
    std::wofstream fileStream;
    std::wstreambuf* oldBuffer;

public:
    StdoutRedirector() : oldBuffer(nullptr) {}

    bool RedirectToFile(const std::wstring& filePath) {
        fileStream.open(filePath, std::ios::out | std::ios::trunc);
        if (!fileStream.is_open()) return false;
        
        fileStream.imbue(std::locale(""));
        oldBuffer = std::wcout.rdbuf();
        std::wcout.rdbuf(fileStream.rdbuf());
        return true;
    }

    ~StdoutRedirector() {
        if (oldBuffer) {
            std::wcout.rdbuf(oldBuffer);
        }
        if (fileStream.is_open()) {
            fileStream.close();
        }
    }
};

// --- Connection Helper with Remote Auth Support ---

std::wstring DescribeWmiHResult(HRESULT hr);

std::wstring EncodePowerShellCommandBase64(const std::wstring& script);

class ScopedEnvironmentVariable {
public:
    ScopedEnvironmentVariable(const wchar_t* name, const std::wstring& value)
        : name_(name), hadOriginal_(false) {
        std::wstring original;
        if (TryGetEnvironmentVariableValue(name_.c_str(), original)) {
            hadOriginal_ = true;
            originalValue_ = original;
        }
        SetEnvironmentVariableW(name_.c_str(), value.c_str());
    }

    ~ScopedEnvironmentVariable() {
        if (hadOriginal_) {
            SetEnvironmentVariableW(name_.c_str(), originalValue_.c_str());
        } else {
            SetEnvironmentVariableW(name_.c_str(), nullptr);
        }
    }

private:
    std::wstring name_;
    std::wstring originalValue_;
    bool hadOriginal_;
};

class ScopedTempFile {
public:
    ScopedTempFile() : handle_(INVALID_HANDLE_VALUE) {}

    ScopedTempFile(const ScopedTempFile&) = delete;
    ScopedTempFile& operator=(const ScopedTempFile&) = delete;

    ~ScopedTempFile() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        if (!path_.empty()) DeleteFileW(path_.c_str());
    }

    void Adopt(const std::wstring& path, HANDLE handle) {
        path_ = path;
        handle_ = handle;
    }

    const std::wstring& Path() const {
        return path_;
    }

    HANDLE Handle() const {
        return handle_;
    }

    bool SealForReading() {
        if (handle_ == INVALID_HANDLE_VALUE || path_.empty()) return false;
        HANDLE readHandle = CreateFileW(
            path_.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_OPEN_REPARSE_POINT,
            NULL
        );
        if (readHandle == INVALID_HANDLE_VALUE) return false;
        CloseHandle(handle_);
        handle_ = readHandle;
        return true;
    }

private:
    std::wstring path_;
    HANDLE handle_;
};

class IWinRmBackend {
public:
    virtual ~IWinRmBackend() {}

    virtual bool ExecuteQuery(
        const WmiTargetConfig& target,
        const std::wstring& wqlQuery,
        const std::vector<std::wstring>& properties,
        const OutputOptions& outputOptions,
        std::wstring& output,
        bool& noData,
        std::wstring& error
    ) const = 0;

    virtual bool ExecuteMethodCall(
        const WmiTargetConfig& target,
        const std::wstring& classNameOrPath,
        const std::wstring& methodName,
        const std::map<std::wstring, std::wstring>& inputArgs,
        std::wstring& output,
        std::wstring& error
    ) const = 0;

    virtual bool ExecuteSetProperty(
        const WmiTargetConfig& target,
        const std::wstring& objectPath,
        const std::wstring& propName,
        const std::wstring& propValue,
        std::wstring& output,
        std::wstring& error
    ) const = 0;

    virtual bool ExecuteDeleteInstance(
        const WmiTargetConfig& target,
        const std::wstring& objectPath,
        std::wstring& output,
        std::wstring& error
    ) const = 0;
};

class WinRmPowerShellBackend final : public IWinRmBackend {
private:
    static std::wstring BuildSessionBlock(const WmiTargetConfig& target) {
        std::wstring block;
        block += L"$opts=@{};";
        if (!(target.node.empty() || target.node == L".")) {
            block += L"$opts.ComputerName='" + EscapePowerShellSingleQuoted(target.node) + L"';";
        }
        if (!target.wmiNamespace.empty()) {
            block += L"$opts.Namespace='" + EscapePowerShellSingleQuoted(target.wmiNamespace) + L"';";
        }
        return block;
    }

    static bool BuildInstanceResolutionQuery(const std::wstring& objectPath, std::wstring& queryOut, std::wstring& errorOut) {
        if (!TryBuildInstanceSelectQueryFromObjectPath(objectPath, queryOut)) {
            errorOut = L"WinRM path operations require object paths in the form Class.Key=Value[,Key=Value].";
            return false;
        }
        return true;
    }

public:
    bool ExecuteQuery(
        const WmiTargetConfig& target,
        const std::wstring& wqlQuery,
        const std::vector<std::wstring>& properties,
        const OutputOptions& outputOptions,
        std::wstring& output,
        bool& noData,
        std::wstring& error
    ) const override {
        if (!target.user.empty() || !target.password.empty() || !target.domain.empty()) {
            error = L"WinRM PowerShell backend does not accept explicit credentials. Use current logon token, or switch to --winrm-backend native.";
            return false;
        }

        auto buildSelectList = [&]() -> std::wstring {
            if (properties.empty()) {
                return L"*";
            }
            std::wstring list;
            for (size_t i = 0; i < properties.size(); ++i) {
                if (i > 0) list += L",";
                list += L"'" + EscapePowerShellSingleQuoted(properties[i]) + L"'";
            }
            return list;
        };

        std::wstring script = BuildSessionBlock(target);
        script += L"$rows=Get-CimInstance @opts -Query '" + EscapePowerShellSingleQuoted(wqlQuery) + L"'";
        script += L"| Select-Object -Property " + buildSelectList() + L";";
        script += L"if($null -eq $rows){$rows=@();}";

        if (outputOptions.format == OutputFormat::Json) {
            script += L"$rows | ConvertTo-Json -Depth 6 -Compress";
        } else if (outputOptions.format == OutputFormat::Csv) {
            script += L"$csv=$rows | ConvertTo-Csv -NoTypeInformation;";
            if (outputOptions.noHeader) {
                script += L"if($csv.Count -gt 1){$csv = $csv[1..($csv.Count-1)]}else{$csv=@()};";
            }
            script += L"$csv";
        } else {
            script += L"$rows | Format-Table -AutoSize";
        }

        int code = -1;
        if (!RunPowerShellCommandCapture(script, output, code) || code != 0) {
            error = L"WinRM PowerShell query backend failed.";
            return false;
        }

        noData = output.empty() || output.find_first_not_of(L" \t\r\n") == std::wstring::npos;
        return true;
    }

    bool ExecuteMethodCall(
        const WmiTargetConfig& target,
        const std::wstring& classNameOrPath,
        const std::wstring& methodName,
        const std::map<std::wstring, std::wstring>& inputArgs,
        std::wstring& output,
        std::wstring& error
    ) const override {
        if (!target.user.empty() || !target.password.empty() || !target.domain.empty()) {
            error = L"WinRM PowerShell backend does not accept explicit credentials. Use current logon token, or switch to --winrm-backend native.";
            return false;
        }

        std::wstring argScript = L"$argMap=@{};";
        for (const auto& kv : inputArgs) {
            argScript += L"$argMap['" + EscapePowerShellSingleQuoted(kv.first) + L"']='" + EscapePowerShellSingleQuoted(kv.second) + L"';";
        }

        std::wstring script = BuildSessionBlock(target);
        script += argScript;
        script += L"$r=Invoke-CimMethod @opts -ClassName '" + EscapePowerShellSingleQuoted(classNameOrPath) + L"' -MethodName '" + EscapePowerShellSingleQuoted(methodName) + L"' -Arguments $argMap;";
        script += L"if($null -ne $r -and $null -ne $r.ReturnValue){'ReturnValue=' + $r.ReturnValue}else{'Method Executed Successfully'}";

        int code = -1;
        if (!RunPowerShellCommandCapture(script, output, code) || code != 0) {
            error = L"WinRM PowerShell method backend failed.";
            return false;
        }

        return true;
    }

    bool ExecuteDeleteInstance(
        const WmiTargetConfig& target,
        const std::wstring& objectPath,
        std::wstring& output,
        std::wstring& error
    ) const override {
        if (!target.user.empty() || !target.password.empty() || !target.domain.empty()) {
            error = L"WinRM PowerShell backend does not accept explicit credentials. Use current logon token, or switch to --winrm-backend native.";
            return false;
        }

        std::wstring query;
        if (!BuildInstanceResolutionQuery(objectPath, query, error)) {
            return false;
        }

        std::wstring script = BuildSessionBlock(target);
        script += L"$ErrorActionPreference='Stop';";
        script += L"try{";
        script += L"$inst=Get-CimInstance @opts -Query '" + EscapePowerShellSingleQuoted(query) + L"' | Select-Object -First 1;";
        script += L"if($null -eq $inst){throw 'Instance not found for delete operation.'};";
        script += L"Remove-CimInstance -InputObject $inst -ErrorAction Stop;";
        script += L"'Instance deleted successfully.'";
        script += L"}catch{ Write-Error $_; exit 3 }";

        int code = -1;
        if (!RunPowerShellCommandCapture(script, output, code) || code != 0) {
            error = L"WinRM PowerShell delete backend failed.";
            std::wstring details = TrimWhitespace(output);
            if (!details.empty()) {
                error += L" Details: " + details;
            }
            return false;
        }

        return true;
    }

    bool ExecuteSetProperty(
        const WmiTargetConfig& target,
        const std::wstring& objectPath,
        const std::wstring& propName,
        const std::wstring& propValue,
        std::wstring& output,
        std::wstring& error
    ) const override {
        if (!target.user.empty() || !target.password.empty() || !target.domain.empty()) {
            error = L"WinRM PowerShell backend does not accept explicit credentials. Use current logon token, or switch to --winrm-backend native.";
            return false;
        }

        std::wstring query;
        if (!BuildInstanceResolutionQuery(objectPath, query, error)) {
            return false;
        }

        std::wstring script = BuildSessionBlock(target);
        script += L"$ErrorActionPreference='Stop';";
        script += L"try{";
        script += L"$inst=Get-CimInstance @opts -Query '" + EscapePowerShellSingleQuoted(query) + L"' | Select-Object -First 1;";
        script += L"if($null -eq $inst){throw 'Instance not found for set operation.'};";
        script += L"$prop=@{ '" + EscapePowerShellSingleQuoted(propName) + L"'='" + EscapePowerShellSingleQuoted(propValue) + L"' };";
        script += L"Set-CimInstance -InputObject $inst -Property $prop -ErrorAction Stop | Out-Null;";
        script += L"'Property updated successfully.'";
        script += L"}catch{ Write-Error $_; exit 3 }";

        int code = -1;
        if (!RunPowerShellCommandCapture(script, output, code) || code != 0) {
            error = L"WinRM PowerShell set backend failed.";
            std::wstring details = TrimWhitespace(output);
            if (!details.empty()) {
                error += L" Details: " + details;
            }
            return false;
        }

        return true;
    }
};

class SshPowerShellBackend final {
private:
    static std::wstring BuildQueryScript(
        const WmiTargetConfig& target,
        const std::wstring& wqlQuery,
        const std::vector<std::wstring>& properties
    ) {
        auto buildSelectList = [&]() -> std::wstring {
            if (properties.empty()) {
                return L"*";
            }

            std::wstring list;
            for (size_t i = 0; i < properties.size(); ++i) {
                if (i > 0) list += L",";
                list += L"'" + EscapePowerShellSingleQuoted(properties[i]) + L"'";
            }
            return list;
        };

        std::wstring script;
        script += L"$rows=Get-CimInstance -Namespace '" + EscapePowerShellSingleQuoted(target.wmiNamespace) + L"' -Query '" + EscapePowerShellSingleQuoted(wqlQuery) + L"'";
        script += L"| Select-Object -Property " + buildSelectList() + L";";
        script += L"if($null -eq $rows){$rows=@();}";

        script += L"$rows | ConvertTo-Json -Depth 6 -Compress";

        return script;
    }

    static bool RenderJsonQueryOutputLocally(
        const std::wstring& jsonText,
        const OutputOptions& outputOptions,
        std::wstring& formattedOutput,
        bool& noData,
        std::wstring& error
    ) {
        formattedOutput.clear();
        noData = false;

        std::wstring trimmed = TrimWhitespace(jsonText);
        if (trimmed.empty() || trimmed == L"[]") {
            noData = true;
            return true;
        }

        ScopedTempFile tempFile;
        if (!CreateUniqueTempFile(L"xmic-ssh-json", L".json", tempFile)) {
            error = L"Failed to create temporary JSON file for SSH output rendering.";
            return false;
        }

        if (!WriteUtf8TextFile(tempFile, trimmed)) {
            error = L"Failed to write temporary JSON file for SSH output rendering.";
            return false;
        }

        std::wstring script = L"$ErrorActionPreference='Stop';";
        script += L"$rows=Get-Content -Raw -Encoding utf8 -LiteralPath '" + EscapePowerShellSingleQuoted(tempFile.Path()) + L"' | ConvertFrom-Json;";
        script += L"if($null -eq $rows){$rows=@()};";

        if (outputOptions.format == OutputFormat::Csv) {
            script += L"$csv=$rows | ConvertTo-Csv -NoTypeInformation;";
            if (outputOptions.noHeader) {
                script += L"if($csv.Count -gt 1){$csv=$csv[1..($csv.Count-1)]}else{$csv=@()};";
            }
            script += L"$csv";
        } else {
            script += L"$rows | Format-Table -AutoSize | Out-String -Width 4096";
        }

        int code = -1;
        if (!RunPowerShellCommandCapture(script, formattedOutput, code) || code != 0) {
            error = L"Failed to render SSH JSON output locally.";
            return false;
        }

        noData = formattedOutput.empty() || formattedOutput.find_first_not_of(L" \t\r\n") == std::wstring::npos;
        return true;
    }

    static std::wstring BuildMethodScript(
        const WmiTargetConfig& target,
        const std::wstring& classNameOrPath,
        const std::wstring& methodName,
        const std::map<std::wstring, std::wstring>& inputArgs
    ) {
        std::wstring argScript = L"$argMap=@{};";
        for (const auto& kv : inputArgs) {
            argScript += L"$argMap['" + EscapePowerShellSingleQuoted(kv.first) + L"']='" + EscapePowerShellSingleQuoted(kv.second) + L"';";
        }

        std::wstring script = argScript;
        script += L"$r=Invoke-CimMethod -Namespace '" + EscapePowerShellSingleQuoted(target.wmiNamespace) +
            L"' -ClassName '" + EscapePowerShellSingleQuoted(classNameOrPath) +
            L"' -MethodName '" + EscapePowerShellSingleQuoted(methodName) + L"' -Arguments $argMap;";
        script += L"if($null -ne $r -and $null -ne $r.ReturnValue){'ReturnValue=' + $r.ReturnValue}else{'Method Executed Successfully'}";
        return script;
    }

    static std::wstring BuildObjectResolutionQuery(const std::wstring& objectPath) {
        std::wstring query;
        if (!TryBuildInstanceSelectQueryFromObjectPath(objectPath, query)) {
            return L"";
        }
        return query;
    }

    static std::wstring BuildSetScript(
        const WmiTargetConfig& target,
        const std::wstring& objectPath,
        const std::wstring& propName,
        const std::wstring& propValue
    ) {
        std::wstring query = BuildObjectResolutionQuery(objectPath);
        if (query.empty()) {
            return L"";
        }

        std::wstring script;
        script += L"$ErrorActionPreference='Stop';";
        script += L"$inst=Get-CimInstance -Namespace '" + EscapePowerShellSingleQuoted(target.wmiNamespace) + L"' -Query '" + EscapePowerShellSingleQuoted(query) + L"' | Select-Object -First 1;";
        script += L"if($null -eq $inst){throw 'Instance not found for set operation.'};";
        script += L"$value='" + EscapePowerShellSingleQuoted(propValue) + L"';";
        script += L"$prop='" + EscapePowerShellSingleQuoted(propName) + L"';";
        script += L"Set-CimInstance -InputObject $inst -Property @{ $prop = $value } -ErrorAction Stop | Out-Null;";
        script += L"'Property updated successfully.'";
        return script;
    }

    static std::wstring BuildDeleteScript(
        const WmiTargetConfig& target,
        const std::wstring& objectPath
    ) {
        std::wstring query = BuildObjectResolutionQuery(objectPath);
        if (query.empty()) {
            return L"";
        }

        std::wstring script;
        script += L"$ErrorActionPreference='Stop';";
        script += L"$inst=Get-CimInstance -Namespace '" + EscapePowerShellSingleQuoted(target.wmiNamespace) + L"' -Query '" + EscapePowerShellSingleQuoted(query) + L"' | Select-Object -First 1;";
        script += L"if($null -eq $inst){throw 'Instance not found for delete operation.'};";
        script += L"Remove-CimInstance -InputObject $inst -ErrorAction Stop;";
        script += L"'Instance deleted successfully.'";
        return script;
    }

public:
    static bool ExecuteQuery(
        const WmiTargetConfig& target,
        const std::wstring& wqlQuery,
        const std::vector<std::wstring>& properties,
        const OutputOptions& outputOptions,
        std::wstring& output,
        bool& noData,
        std::wstring& error
    ) {
        std::wstring script = BuildQueryScript(target, wqlQuery, properties);
        int code = -1;
        if (!RunSshPowerShellScriptCapture(target, script, output, code) || code != 0) {
            error = L"SSH PowerShell query backend failed.";
            return false;
        }

        if (outputOptions.format == OutputFormat::Json) {
            noData = output.empty() || output.find_first_not_of(L" \t\r\n") == std::wstring::npos || TrimWhitespace(output) == L"[]";
            if (!noData) {
                output = TrimWhitespace(output);
                if (output.empty() || output.back() != L'\n') {
                    output += L"\n";
                }
            }
            return true;
        }

        std::wstring formatted;
        std::wstring renderError;
        if (!RenderJsonQueryOutputLocally(output, outputOptions, formatted, noData, renderError)) {
            error = renderError;
            return false;
        }

        output = formatted;
        return true;
    }

    static bool ExecuteMethodCall(
        const WmiTargetConfig& target,
        const std::wstring& classNameOrPath,
        const std::wstring& methodName,
        const std::map<std::wstring, std::wstring>& inputArgs,
        std::wstring& output,
        std::wstring& error
    ) {
        std::wstring script = BuildMethodScript(target, classNameOrPath, methodName, inputArgs);
        int code = -1;
        if (!RunSshPowerShellScriptCapture(target, script, output, code) || code != 0) {
            error = L"SSH PowerShell method backend failed.";
            return false;
        }

        return true;
    }

    static bool ExecuteSetProperty(
        const WmiTargetConfig& target,
        const std::wstring& objectPath,
        const std::wstring& propName,
        const std::wstring& propValue,
        std::wstring& output,
        std::wstring& error
    ) {
        std::wstring script = BuildSetScript(target, objectPath, propName, propValue);
        if (script.empty()) {
            error = L"SSH path set requires object paths in the form Class.Key=Value[,Key=Value].";
            return false;
        }

        int code = -1;
        if (!RunSshPowerShellScriptCapture(target, script, output, code) || code != 0) {
            error = L"SSH path set failed.";
            return false;
        }

        return true;
    }

    static bool ExecuteDeleteInstance(
        const WmiTargetConfig& target,
        const std::wstring& objectPath,
        std::wstring& output,
        std::wstring& error
    ) {
        std::wstring script = BuildDeleteScript(target, objectPath);
        if (script.empty()) {
            error = L"SSH path delete requires object paths in the form Class.Key=Value[,Key=Value].";
            return false;
        }

        int code = -1;
        if (!RunSshPowerShellScriptCapture(target, script, output, code) || code != 0) {
            error = L"SSH path delete failed.";
            return false;
        }

        return true;
    }
};

class WinRmNativeBackendStub final : public IWinRmBackend {
private:
    static std::vector<std::wstring> SplitCsvLikeValues(const std::wstring& input) {
        return SplitCommaTrimmed(input);
    }

    class ScopedMiOperation {
    public:
        MI_Operation operation = MI_OPERATION_NULL;

        ~ScopedMiOperation() {
            Close();
        }

        void Close() {
            if (operation.ft) {
                MI_Operation_Close(&operation);
                memset(&operation, 0, sizeof(operation));
            }
        }
    };

    class ScopedMiSession {
    public:
        MI_Application application = MI_APPLICATION_NULL;
        MI_DestinationOptions destinationOptions = MI_DESTINATIONOPTIONS_NULL;
        MI_Session session = MI_SESSION_NULL;

        ~ScopedMiSession() {
            if (session.ft) MI_Session_Close(&session, NULL, NULL);
            if (destinationOptions.ft) MI_DestinationOptions_Delete(&destinationOptions);
            if (application.ft) MI_Application_Close(&application);
        }
    };

    class ScopedMiInstance {
    public:
        MI_Instance* instance = NULL;

        ~ScopedMiInstance() {
            Reset();
        }

        void Reset() {
            if (instance) {
                MI_Instance_Delete(instance);
                instance = NULL;
            }
        }
    };

    static std::wstring FormatMiDateTime(const MI_Datetime& dt) {
        if (dt.isTimestamp) {
            const MI_Timestamp& ts = dt.u.timestamp;
            wchar_t buf[64];
            swprintf_s(buf, L"%04u-%02u-%02u %02u:%02u:%02u", ts.year, ts.month, ts.day, ts.hour, ts.minute, ts.second);
            return std::wstring(buf);
        }

        const MI_Interval& iv = dt.u.interval;
        return std::to_wstring(iv.days) + L"d " +
               std::to_wstring(iv.hours) + L"h " +
               std::to_wstring(iv.minutes) + L"m " +
               std::to_wstring(iv.seconds) + L"s";
    }

    static std::wstring JoinArrayItems(const std::vector<std::wstring>& items) {
        if (items.empty()) {
            return L"N/A";
        }

        std::wstring out;
        size_t total = 0;
        for (const auto& item : items) {
            total += item.size();
        }
        if (items.size() > 1) {
            total += (items.size() - 1) * 2;
        }
        out.reserve(total);

        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) out += L", ";
            out += items[i];
        }
        return out;
    }

    static std::wstring MiValueToString(const MI_Value& value, MI_Type type, MI_Uint32 flags) {
        if (flags & MI_FLAG_NULL) {
            return L"N/A";
        }

        switch (type) {
        case MI_STRING:
            return value.string ? std::wstring(value.string) : L"N/A";
        case MI_BOOLEAN:
            return value.boolean ? L"True" : L"False";
        case MI_UINT8:
            return std::to_wstring(value.uint8);
        case MI_SINT8:
            return std::to_wstring(value.sint8);
        case MI_UINT16:
            return std::to_wstring(value.uint16);
        case MI_SINT16:
            return std::to_wstring(value.sint16);
        case MI_UINT32:
            return std::to_wstring(value.uint32);
        case MI_SINT32:
            return std::to_wstring(value.sint32);
        case MI_UINT64:
            return std::to_wstring(value.uint64);
        case MI_SINT64:
            return std::to_wstring(value.sint64);
        case MI_REAL32:
            return std::to_wstring(value.real32);
        case MI_REAL64:
            return std::to_wstring(value.real64);
        case MI_CHAR16:
            return std::wstring(1, static_cast<wchar_t>(value.char16));
        case MI_DATETIME:
            return FormatMiDateTime(value.datetime);
        case MI_REFERENCE:
            return L"<Reference>";
        case MI_INSTANCE:
            return L"<Instance>";
        case MI_STRINGA: {
            std::vector<std::wstring> items;
            for (MI_Uint32 i = 0; i < value.stringa.size; ++i) {
                items.push_back(value.stringa.data[i] ? std::wstring(value.stringa.data[i]) : L"");
            }
            return JoinArrayItems(items);
        }
        case MI_UINT32A: {
            std::vector<std::wstring> items;
            for (MI_Uint32 i = 0; i < value.uint32a.size; ++i) items.push_back(std::to_wstring(value.uint32a.data[i]));
            return JoinArrayItems(items);
        }
        case MI_SINT32A: {
            std::vector<std::wstring> items;
            for (MI_Uint32 i = 0; i < value.sint32a.size; ++i) items.push_back(std::to_wstring(value.sint32a.data[i]));
            return JoinArrayItems(items);
        }
        case MI_UINT64A: {
            std::vector<std::wstring> items;
            for (MI_Uint32 i = 0; i < value.uint64a.size; ++i) items.push_back(std::to_wstring(value.uint64a.data[i]));
            return JoinArrayItems(items);
        }
        case MI_SINT64A: {
            std::vector<std::wstring> items;
            for (MI_Uint32 i = 0; i < value.sint64a.size; ++i) items.push_back(std::to_wstring(value.sint64a.data[i]));
            return JoinArrayItems(items);
        }
        case MI_BOOLEANA: {
            std::vector<std::wstring> items;
            for (MI_Uint32 i = 0; i < value.booleana.size; ++i) items.push_back(value.booleana.data[i] ? L"True" : L"False");
            return JoinArrayItems(items);
        }
        case MI_DATETIMEA: {
            std::vector<std::wstring> items;
            for (MI_Uint32 i = 0; i < value.datetimea.size; ++i) items.push_back(FormatMiDateTime(value.datetimea.data[i]));
            return JoinArrayItems(items);
        }
        default:
            return L"<Data>";
        }
    }

    static std::wstring FormatJsonValueToken(const std::wstring& rawVal) {
        if (rawVal.empty() || rawVal == L"N/A") {
            return L"null";
        }

        std::wstring lowered = rawVal;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);

        if (lowered == L"true" || lowered == L"false") {
            return lowered;
        }

        if (IsIntegerLiteral(rawVal)) {
            return rawVal;
        }

        return L"\"" + EscapeJson(rawVal) + L"\"";
    }

    static std::wstring BuildOutputString(
        const std::vector<std::wstring>& headers,
        const std::vector<std::vector<std::wstring>>& rows,
        const OutputOptions& outputOptions
    ) {
        if (outputOptions.format == OutputFormat::Csv) {
            std::wstring out;
            out.reserve((headers.size() * 16) + (rows.size() * (headers.size() * 16 + 1)) + 16);
            const std::wstring emptyCell;

            if (!outputOptions.noHeader) {
                for (size_t i = 0; i < headers.size(); ++i) {
                    if (i > 0) out += L",";
                    out += EscapeCsv(headers[i]);
                }
                out += L"\n";
            }
            for (const auto& row : rows) {
                for (size_t i = 0; i < headers.size(); ++i) {
                    if (i > 0) out += L",";
                    const std::wstring& cell = (i < row.size()) ? row[i] : emptyCell;
                    out += EscapeCsv(cell);
                }
                out += L"\n";
            }
            return out;
        }

        if (outputOptions.format == OutputFormat::Json) {
            std::wstring out;
            out.reserve((headers.size() * 16) + (rows.size() * (headers.size() * 24 + 4)) + 16);
            out += L"[";
            for (size_t r = 0; r < rows.size(); ++r) {
                if (r > 0) out += L",";
                out += L"{";
                for (size_t c = 0; c < headers.size(); ++c) {
                    if (c > 0) out += L",";
                    std::wstring key = EscapeJson(headers[c]);
                    std::wstring val = (c < rows[r].size()) ? rows[r][c] : L"";
                    out += L"\"";
                    out += key;
                    out += L"\":";
                    out += FormatJsonValueToken(val);
                }
                out += L"}";
            }
            out += L"]\n";
            return out;
        }

        std::wstringstream out;

        std::vector<size_t> widths(headers.size(), 0);
        for (size_t i = 0; i < headers.size(); ++i) widths[i] = headers[i].size();
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
                widths[i] = (std::max)(widths[i], row[i].size());
            }
        }

        out << L"\n";
        if (!outputOptions.noHeader) {
            for (size_t i = 0; i < headers.size(); ++i) {
                out << std::left << std::setw(static_cast<int>(widths[i] + 3)) << headers[i];
            }
            out << L"\n";
            size_t totalWidth = 0;
            for (size_t w : widths) totalWidth += w + 3;
            out << std::wstring(totalWidth, L'-') << L"\n";
        }

        for (const auto& row : rows) {
            const std::wstring emptyCell;
            for (size_t i = 0; i < headers.size(); ++i) {
                const std::wstring& cell = (i < row.size()) ? row[i] : emptyCell;
                out << std::left << std::setw(static_cast<int>(widths[i] + 3)) << cell;
            }
            out << L"\n";
        }
        out << L"\n";
        return out.str();
    }

public:
    bool ExecuteQuery(
        const WmiTargetConfig& target,
        const std::wstring& wqlQuery,
        const std::vector<std::wstring>& properties,
        const OutputOptions& outputOptions,
        std::wstring& output,
        bool& noData,
        std::wstring& error
    ) const override {
        ScopedMiSession miScope;
        ScopedMiOperation operation;
        ScopedMiInstance extendedError;

        MI_Result miResult = MI_Application_Initialize(0, NULL, &extendedError.instance, &miScope.application);
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_Initialize failed.";
            return false;
        }

        miResult = MI_Application_NewDestinationOptions(&miScope.application, &miScope.destinationOptions);
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_NewDestinationOptions failed.";
            return false;
        }

        if (!target.user.empty()) {
            MI_UserCredentials creds;
            memset(&creds, 0, sizeof(creds));
            creds.authenticationType = MI_AUTH_TYPE_NEGO_WITH_CREDS;
            creds.credentials.usernamePassword.domain = target.domain.empty() ? NULL : target.domain.c_str();
            creds.credentials.usernamePassword.username = target.user.c_str();
            creds.credentials.usernamePassword.password = target.password.c_str();
            miResult = MI_DestinationOptions_AddDestinationCredentials(&miScope.destinationOptions, &creds);
            if (miResult != MI_RESULT_OK) {
                error = L"MI_DestinationOptions_AddDestinationCredentials failed.";
                return false;
            }
        }

        const MI_Char* destination = (target.node.empty() || target.node == L".") ? NULL : target.node.c_str();
        miResult = MI_Application_NewSession(
            &miScope.application,
            MI_T("WINRM"),
            destination,
            &miScope.destinationOptions,
            NULL,
            &extendedError.instance,
            &miScope.session
        );
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_NewSession failed.";
            return false;
        }

        MI_Session_QueryInstances(
            &miScope.session,
            0,
            NULL,
            target.wmiNamespace.c_str(),
            MI_T("WQL"),
            wqlQuery.c_str(),
            NULL,
            &operation.operation
        );

        std::vector<std::wstring> headers = properties;
        std::vector<std::vector<std::wstring>> rows;

        for (;;) {
            const MI_Instance* instance = NULL;
            MI_Boolean moreResults = MI_FALSE;
            MI_Result operationResult = MI_RESULT_OK;
            const MI_Char* errorMessage = NULL;
            const MI_Instance* completionDetails = NULL;

            miResult = MI_Operation_GetInstance(
                &operation.operation,
                &instance,
                &moreResults,
                &operationResult,
                &errorMessage,
                &completionDetails
            );

            if (miResult != MI_RESULT_OK) {
                error = L"MI_Operation_GetInstance failed.";
                return false;
            }

            if (instance) {
                if (headers.empty()) {
                    MI_Uint32 count = 0;
                    if (MI_Instance_GetElementCount(instance, &count) == MI_RESULT_OK) {
                        for (MI_Uint32 i = 0; i < count; ++i) {
                            const MI_Char* name = NULL;
                            if (MI_Instance_GetElementAt(instance, i, &name, NULL, NULL, NULL) == MI_RESULT_OK && name) {
                                headers.push_back(name);
                            }
                        }
                    }
                }

                std::vector<std::wstring> row;
                row.reserve(headers.size());
                for (const auto& prop : headers) {
                    MI_Value value;
                    memset(&value, 0, sizeof(value));
                    MI_Type type = MI_STRING;
                    MI_Uint32 flags = 0;
                    if (MI_Instance_GetElement(instance, prop.c_str(), &value, &type, &flags, NULL) == MI_RESULT_OK) {
                        row.push_back(MiValueToString(value, type, flags));
                    } else {
                        row.push_back(L"N/A");
                    }
                }
                rows.push_back(std::move(row));
            }

            if (!moreResults) {
                if (operationResult != MI_RESULT_OK) {
                    error = errorMessage ? std::wstring(errorMessage) : L"MI query operation failed.";
                    return false;
                }
                break;
            }
        }

        noData = rows.empty();
        output = noData ? L"" : BuildOutputString(headers, rows, outputOptions);
        return true;
    }

    bool ExecuteMethodCall(
        const WmiTargetConfig& target,
        const std::wstring& classNameOrPath,
        const std::wstring& methodName,
        const std::map<std::wstring, std::wstring>& inputArgs,
        std::wstring& output,
        std::wstring& error
    ) const override {
        ScopedMiSession miScope;
        ScopedMiOperation classOperation;
        ScopedMiOperation invokeOperation;
        ScopedMiInstance extendedError;
        ScopedMiInstance invokeInputParams;

        auto extractClassName = [](const std::wstring& raw) -> std::wstring {
            if (raw.find(L'=') != std::wstring::npos) {
                size_t dotPos = raw.find(L'.');
                if (dotPos != std::wstring::npos && dotPos > 0) {
                    return raw.substr(0, dotPos);
                }
            }
            return raw;
        };

        auto parseBoolean = [](const std::wstring& raw, MI_Boolean& out) -> bool {
            std::wstring lowered = raw;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
            if (lowered == L"true" || lowered == L"1") {
                out = MI_TRUE;
                return true;
            }
            if (lowered == L"false" || lowered == L"0") {
                out = MI_FALSE;
                return true;
            }
            return false;
        };

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

        auto addTypedElement = [&](MI_Instance* targetInstance, const std::wstring& name, MI_Type declaredType, const std::wstring& rawValue, bool hasDeclaredType) -> bool {
            auto add = [&](const MI_Value& value, MI_Type type) -> bool {
                return MI_Instance_AddElement(targetInstance, name.c_str(), &value, type, 0) == MI_RESULT_OK;
            };

            auto addFallback = [&]() -> bool {
                if (isIntegerLiteral(rawValue)) {
                    try {
                        MI_Value value;
                        memset(&value, 0, sizeof(value));
                        value.sint64 = static_cast<MI_Sint64>(std::stoll(rawValue));
                        if (add(value, MI_SINT64)) {
                            return true;
                        }
                    } catch (...) {
                    }
                }

                MI_Boolean boolVal;
                if (parseBoolean(rawValue, boolVal)) {
                    MI_Value value;
                    memset(&value, 0, sizeof(value));
                    value.boolean = boolVal;
                    if (add(value, MI_BOOLEAN)) {
                        return true;
                    }
                }

                MI_Value value;
                memset(&value, 0, sizeof(value));
                value.string = const_cast<MI_Char*>(rawValue.c_str());
                return add(value, MI_STRING);
            };

            if (!hasDeclaredType) {
                return addFallback();
            }

            try {
                switch (declaredType) {
                case MI_SINT8: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.sint8 = static_cast<MI_Sint8>(std::stoi(rawValue));
                    return add(value, MI_SINT8);
                }
                case MI_UINT8: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.uint8 = static_cast<MI_Uint8>(std::stoul(rawValue));
                    return add(value, MI_UINT8);
                }
                case MI_SINT16: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.sint16 = static_cast<MI_Sint16>(std::stoi(rawValue));
                    return add(value, MI_SINT16);
                }
                case MI_UINT16: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.uint16 = static_cast<MI_Uint16>(std::stoul(rawValue));
                    return add(value, MI_UINT16);
                }
                case MI_SINT32: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.sint32 = static_cast<MI_Sint32>(std::stol(rawValue));
                    return add(value, MI_SINT32);
                }
                case MI_UINT32: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.uint32 = static_cast<MI_Uint32>(std::stoul(rawValue));
                    return add(value, MI_UINT32);
                }
                case MI_SINT64: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.sint64 = static_cast<MI_Sint64>(std::stoll(rawValue));
                    return add(value, MI_SINT64);
                }
                case MI_UINT64: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.uint64 = static_cast<MI_Uint64>(std::stoull(rawValue));
                    return add(value, MI_UINT64);
                }
                case MI_REAL32: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.real32 = static_cast<MI_Real32>(std::stof(rawValue));
                    return add(value, MI_REAL32);
                }
                case MI_REAL64: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.real64 = static_cast<MI_Real64>(std::stod(rawValue));
                    return add(value, MI_REAL64);
                }
                case MI_BOOLEAN: {
                    MI_Boolean boolVal;
                    if (!parseBoolean(rawValue, boolVal)) {
                        return false;
                    }
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.boolean = boolVal;
                    return add(value, MI_BOOLEAN);
                }
                case MI_CHAR16: {
                    if (rawValue.empty()) {
                        return false;
                    }
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.char16 = static_cast<MI_Char16>(rawValue[0]);
                    return add(value, MI_CHAR16);
                }
                case MI_STRING:
                case MI_REFERENCE:
                case MI_DATETIME: {
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.string = const_cast<MI_Char*>(rawValue.c_str());
                    return add(value, MI_STRING);
                }
                case MI_SINT32A: {
                    std::vector<std::wstring> items = SplitCsvLikeValues(rawValue);
                    std::vector<MI_Sint32> values;
                    values.reserve(items.size());
                    for (const auto& item : items) values.push_back(static_cast<MI_Sint32>(std::stol(item)));
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.sint32a.data = values.data(); value.sint32a.size = static_cast<MI_Uint32>(values.size());
                    return !values.empty() && add(value, MI_SINT32A);
                }
                case MI_UINT32A: {
                    std::vector<std::wstring> items = SplitCsvLikeValues(rawValue);
                    std::vector<MI_Uint32> values;
                    values.reserve(items.size());
                    for (const auto& item : items) values.push_back(static_cast<MI_Uint32>(std::stoul(item)));
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.uint32a.data = values.data(); value.uint32a.size = static_cast<MI_Uint32>(values.size());
                    return !values.empty() && add(value, MI_UINT32A);
                }
                case MI_SINT64A: {
                    std::vector<std::wstring> items = SplitCsvLikeValues(rawValue);
                    std::vector<MI_Sint64> values;
                    values.reserve(items.size());
                    for (const auto& item : items) values.push_back(static_cast<MI_Sint64>(std::stoll(item)));
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.sint64a.data = values.data(); value.sint64a.size = static_cast<MI_Uint32>(values.size());
                    return !values.empty() && add(value, MI_SINT64A);
                }
                case MI_UINT64A: {
                    std::vector<std::wstring> items = SplitCsvLikeValues(rawValue);
                    std::vector<MI_Uint64> values;
                    values.reserve(items.size());
                    for (const auto& item : items) values.push_back(static_cast<MI_Uint64>(std::stoull(item)));
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.uint64a.data = values.data(); value.uint64a.size = static_cast<MI_Uint32>(values.size());
                    return !values.empty() && add(value, MI_UINT64A);
                }
                case MI_BOOLEANA: {
                    std::vector<std::wstring> items = SplitCsvLikeValues(rawValue);
                    std::vector<MI_Boolean> values;
                    values.reserve(items.size());
                    for (const auto& item : items) {
                        MI_Boolean boolVal;
                        if (!parseBoolean(item, boolVal)) {
                            return false;
                        }
                        values.push_back(boolVal);
                    }
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.booleana.data = values.data(); value.booleana.size = static_cast<MI_Uint32>(values.size());
                    return !values.empty() && add(value, MI_BOOLEANA);
                }
                case MI_STRINGA: {
                    std::vector<std::wstring> items = SplitCsvLikeValues(rawValue);
                    std::vector<MI_Char*> values;
                    values.reserve(items.size());
                    for (auto& item : items) values.push_back(const_cast<MI_Char*>(item.c_str()));
                    MI_Value value; memset(&value, 0, sizeof(value));
                    value.stringa.data = values.data(); value.stringa.size = static_cast<MI_Uint32>(values.size());
                    return !values.empty() && add(value, MI_STRINGA);
                }
                default:
                    return addFallback();
                }
            } catch (...) {
                return false;
            }
        };

        std::wstring className = extractClassName(classNameOrPath);

        MI_Result miResult = MI_Application_Initialize(0, NULL, &extendedError.instance, &miScope.application);
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_Initialize failed.";
            return false;
        }

        miResult = MI_Application_NewDestinationOptions(&miScope.application, &miScope.destinationOptions);
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_NewDestinationOptions failed.";
            return false;
        }

        if (!target.user.empty()) {
            MI_UserCredentials creds;
            memset(&creds, 0, sizeof(creds));
            creds.authenticationType = MI_AUTH_TYPE_NEGO_WITH_CREDS;
            creds.credentials.usernamePassword.domain = target.domain.empty() ? NULL : target.domain.c_str();
            creds.credentials.usernamePassword.username = target.user.c_str();
            creds.credentials.usernamePassword.password = target.password.c_str();
            miResult = MI_DestinationOptions_AddDestinationCredentials(&miScope.destinationOptions, &creds);
            if (miResult != MI_RESULT_OK) {
                error = L"MI_DestinationOptions_AddDestinationCredentials failed.";
                return false;
            }
        }

        const MI_Char* destination = (target.node.empty() || target.node == L".") ? NULL : target.node.c_str();
        miResult = MI_Application_NewSession(
            &miScope.application,
            MI_T("WINRM"),
            destination,
            &miScope.destinationOptions,
            NULL,
            &extendedError.instance,
            &miScope.session
        );
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_NewSession failed.";
            return false;
        }

        std::map<std::wstring, MI_Type> methodParameterTypes;
        MI_Session_GetClass(
            &miScope.session,
            0,
            NULL,
            target.wmiNamespace.c_str(),
            className.c_str(),
            NULL,
            &classOperation.operation
        );

        const MI_Class* methodClass = NULL;
        for (;;) {
            const MI_Class* classResult = NULL;
            MI_Boolean moreResults = MI_FALSE;
            MI_Result operationResult = MI_RESULT_OK;
            const MI_Char* errorMessage = NULL;
            const MI_Instance* completionDetails = NULL;

            miResult = MI_Operation_GetClass(
                &classOperation.operation,
                &classResult,
                &moreResults,
                &operationResult,
                &errorMessage,
                &completionDetails
            );

            if (miResult != MI_RESULT_OK) {
                break;
            }

            if (classResult) {
                methodClass = classResult;
            }

            if (!moreResults) {
                break;
            }
        }

        if (methodClass) {
            MI_ParameterSet methodParams;
            memset(&methodParams, 0, sizeof(methodParams));
            miResult = MI_Class_GetMethod(methodClass, methodName.c_str(), NULL, &methodParams, NULL);
            if (miResult == MI_RESULT_OK) {
                MI_Uint32 count = 0;
                if (MI_ParameterSet_GetParameterCount(&methodParams, &count) == MI_RESULT_OK) {
                    for (MI_Uint32 i = 0; i < count; ++i) {
                        const MI_Char* paramName = NULL;
                        MI_Type paramType = MI_STRING;
                        if (MI_ParameterSet_GetParameterAt(&methodParams, i, &paramName, &paramType, NULL, NULL) == MI_RESULT_OK && paramName) {
                            methodParameterTypes[paramName] = paramType;
                        }
                    }
                }
            }
        }

        classOperation.Close();

        if (!inputArgs.empty()) {
            miResult = MI_Application_NewInstance(&miScope.application, className.c_str(), NULL, &invokeInputParams.instance);
            if (miResult != MI_RESULT_OK || !invokeInputParams.instance) {
                error = L"MI_Application_NewInstance failed for method parameters.";
                return false;
            }

            for (const auto& kv : inputArgs) {
                auto it = methodParameterTypes.find(kv.first);
                bool hasType = (it != methodParameterTypes.end());
                MI_Type paramType = hasType ? it->second : MI_STRING;
                if (!addTypedElement(invokeInputParams.instance, kv.first, paramType, kv.second, hasType)) {
                    error = L"Failed to convert method input parameter: " + kv.first;
                    return false;
                }
            }
        }

        MI_Session_Invoke(
            &miScope.session,
            0,
            NULL,
            target.wmiNamespace.c_str(),
            className.c_str(),
            methodName.c_str(),
            NULL,
            invokeInputParams.instance,
            NULL,
            &invokeOperation.operation
        );

        std::vector<std::pair<std::wstring, std::wstring>> outputPairs;
        for (;;) {
            const MI_Instance* instance = NULL;
            MI_Boolean moreResults = MI_FALSE;
            MI_Result operationResult = MI_RESULT_OK;
            const MI_Char* errorMessage = NULL;
            const MI_Instance* completionDetails = NULL;

            miResult = MI_Operation_GetInstance(
                &invokeOperation.operation,
                &instance,
                &moreResults,
                &operationResult,
                &errorMessage,
                &completionDetails
            );

            if (miResult != MI_RESULT_OK) {
                error = L"MI_Operation_GetInstance failed during method invocation.";
                return false;
            }

            if (instance) {
                MI_Uint32 count = 0;
                if (MI_Instance_GetElementCount(instance, &count) == MI_RESULT_OK) {
                    for (MI_Uint32 i = 0; i < count; ++i) {
                        const MI_Char* name = NULL;
                        MI_Value value;
                        memset(&value, 0, sizeof(value));
                        MI_Type type = MI_STRING;
                        MI_Uint32 flags = 0;
                        if (MI_Instance_GetElementAt(instance, i, &name, &value, &type, &flags) == MI_RESULT_OK && name) {
                            outputPairs.push_back({ name, MiValueToString(value, type, flags) });
                        }
                    }
                }
            }

            if (!moreResults) {
                if (operationResult != MI_RESULT_OK) {
                    error = errorMessage ? std::wstring(errorMessage) : L"MI invoke operation failed.";
                    return false;
                }
                break;
            }
        }

        output.clear();
        auto returnIt = std::find_if(outputPairs.begin(), outputPairs.end(), [](const std::pair<std::wstring, std::wstring>& kv) {
            return _wcsicmp(kv.first.c_str(), L"ReturnValue") == 0;
        });

        if (returnIt != outputPairs.end()) {
            output = L"ReturnValue=" + returnIt->second;
            for (const auto& kv : outputPairs) {
                if (_wcsicmp(kv.first.c_str(), L"ReturnValue") == 0) {
                    continue;
                }
                output += L"; " + kv.first + L"=" + kv.second;
            }
        } else {
            for (size_t i = 0; i < outputPairs.size(); ++i) {
                if (i > 0) output += L"; ";
                output += outputPairs[i].first + L"=" + outputPairs[i].second;
            }
        }

        return true;
    }

    bool ExecuteSetProperty(
        const WmiTargetConfig& target,
        const std::wstring& objectPath,
        const std::wstring& propName,
        const std::wstring& propValue,
        std::wstring& output,
        std::wstring& error
    ) const override {
        ScopedMiSession miScope;
        ScopedMiOperation queryOperation;
        ScopedMiOperation modifyOperation;
        ScopedMiInstance extendedError;
        ScopedMiInstance targetInstance;

        auto parseBoolean = [](const std::wstring& raw, MI_Boolean& out) -> bool {
            std::wstring lowered = raw;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(), towlower);
            if (lowered == L"true" || lowered == L"1") {
                out = MI_TRUE;
                return true;
            }
            if (lowered == L"false" || lowered == L"0") {
                out = MI_FALSE;
                return true;
            }
            return false;
        };

        std::wstring query;
        if (!TryBuildInstanceSelectQueryFromObjectPath(objectPath, query)) {
            error = L"WinRM native backend path set requires object paths in the form Class.Key=Value[,Key=Value].";
            return false;
        }

        MI_Result miResult = MI_Application_Initialize(0, NULL, &extendedError.instance, &miScope.application);
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_Initialize failed.";
            return false;
        }

        miResult = MI_Application_NewDestinationOptions(&miScope.application, &miScope.destinationOptions);
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_NewDestinationOptions failed.";
            return false;
        }

        if (!target.user.empty()) {
            MI_UserCredentials creds;
            memset(&creds, 0, sizeof(creds));
            creds.authenticationType = MI_AUTH_TYPE_NEGO_WITH_CREDS;
            creds.credentials.usernamePassword.domain = target.domain.empty() ? NULL : target.domain.c_str();
            creds.credentials.usernamePassword.username = target.user.c_str();
            creds.credentials.usernamePassword.password = target.password.c_str();
            miResult = MI_DestinationOptions_AddDestinationCredentials(&miScope.destinationOptions, &creds);
            if (miResult != MI_RESULT_OK) {
                error = L"MI_DestinationOptions_AddDestinationCredentials failed.";
                return false;
            }
        }

        const MI_Char* destination = (target.node.empty() || target.node == L".") ? NULL : target.node.c_str();
        miResult = MI_Application_NewSession(
            &miScope.application,
            MI_T("WINRM"),
            destination,
            &miScope.destinationOptions,
            NULL,
            &extendedError.instance,
            &miScope.session
        );
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_NewSession failed.";
            return false;
        }

        MI_Session_QueryInstances(
            &miScope.session,
            0,
            NULL,
            target.wmiNamespace.c_str(),
            MI_T("WQL"),
            query.c_str(),
            NULL,
            &queryOperation.operation
        );

        for (;;) {
            const MI_Instance* instance = NULL;
            MI_Boolean moreResults = MI_FALSE;
            MI_Result operationResult = MI_RESULT_OK;
            const MI_Char* errorMessage = NULL;
            const MI_Instance* completionDetails = NULL;

            miResult = MI_Operation_GetInstance(
                &queryOperation.operation,
                &instance,
                &moreResults,
                &operationResult,
                &errorMessage,
                &completionDetails
            );

            if (miResult != MI_RESULT_OK) {
                error = L"MI_Operation_GetInstance failed during target instance resolution.";
                return false;
            }

            if (instance && targetInstance.instance == NULL) {
                if (MI_Instance_Clone(instance, &targetInstance.instance) != MI_RESULT_OK || !targetInstance.instance) {
                    error = L"MI_Instance_Clone failed while preparing set operation.";
                    return false;
                }
            }

            if (!moreResults) {
                if (operationResult != MI_RESULT_OK) {
                    error = errorMessage ? std::wstring(errorMessage) : L"MI query operation failed while resolving set target.";
                    return false;
                }
                break;
            }
        }

        if (!targetInstance.instance) {
            error = L"Instance not found for set operation.";
            return false;
        }

        MI_Value existingValue;
        memset(&existingValue, 0, sizeof(existingValue));
        MI_Type existingType = MI_STRING;
        MI_Uint32 existingFlags = 0;
        if (MI_Instance_GetElement(targetInstance.instance, propName.c_str(), &existingValue, &existingType, &existingFlags, NULL) != MI_RESULT_OK) {
            error = L"Property not found on target instance: " + propName;
            return false;
        }

        MI_Value newValue;
        memset(&newValue, 0, sizeof(newValue));
        bool converted = true;

        try {
            switch (existingType) {
            case MI_BOOLEAN: {
                MI_Boolean b = MI_FALSE;
                if (!parseBoolean(propValue, b)) {
                    converted = false;
                    break;
                }
                newValue.boolean = b;
                break;
            }
            case MI_SINT8: newValue.sint8 = static_cast<MI_Sint8>(std::stoi(propValue)); break;
            case MI_UINT8: newValue.uint8 = static_cast<MI_Uint8>(std::stoul(propValue)); break;
            case MI_SINT16: newValue.sint16 = static_cast<MI_Sint16>(std::stoi(propValue)); break;
            case MI_UINT16: newValue.uint16 = static_cast<MI_Uint16>(std::stoul(propValue)); break;
            case MI_SINT32: newValue.sint32 = static_cast<MI_Sint32>(std::stol(propValue)); break;
            case MI_UINT32: newValue.uint32 = static_cast<MI_Uint32>(std::stoul(propValue)); break;
            case MI_SINT64: newValue.sint64 = static_cast<MI_Sint64>(std::stoll(propValue)); break;
            case MI_UINT64: newValue.uint64 = static_cast<MI_Uint64>(std::stoull(propValue)); break;
            case MI_REAL32: newValue.real32 = static_cast<MI_Real32>(std::stof(propValue)); break;
            case MI_REAL64: newValue.real64 = static_cast<MI_Real64>(std::stod(propValue)); break;
            case MI_CHAR16:
                if (propValue.empty()) {
                    converted = false;
                    break;
                }
                newValue.char16 = static_cast<MI_Char16>(propValue[0]);
                break;
            case MI_STRING:
            case MI_REFERENCE:
            case MI_DATETIME:
            default:
                newValue.string = const_cast<MI_Char*>(propValue.c_str());
                existingType = MI_STRING;
                break;
            }
        } catch (...) {
            converted = false;
        }

        if (!converted) {
            error = L"Failed to convert value for property type on set operation: " + propName;
            return false;
        }

        if (MI_Instance_SetElement(targetInstance.instance, propName.c_str(), &newValue, existingType, 0) != MI_RESULT_OK) {
            error = L"MI_Instance_SetElement failed for property: " + propName;
            return false;
        }

        MI_Session_ModifyInstance(
            &miScope.session,
            0,
            NULL,
            target.wmiNamespace.c_str(),
            targetInstance.instance,
            NULL,
            &modifyOperation.operation
        );

        for (;;) {
            const MI_Instance* instance = NULL;
            MI_Boolean moreResults = MI_FALSE;
            MI_Result operationResult = MI_RESULT_OK;
            const MI_Char* errorMessage = NULL;
            const MI_Instance* completionDetails = NULL;

            miResult = MI_Operation_GetInstance(
                &modifyOperation.operation,
                &instance,
                &moreResults,
                &operationResult,
                &errorMessage,
                &completionDetails
            );

            if (miResult != MI_RESULT_OK) {
                error = L"MI_Operation_GetInstance failed during set operation.";
                return false;
            }

            if (!moreResults) {
                if (operationResult != MI_RESULT_OK) {
                    error = errorMessage ? std::wstring(errorMessage) : L"MI modify operation failed.";
                    return false;
                }
                break;
            }
        }

        output = L"Property updated successfully.";
        return true;
    }

    bool ExecuteDeleteInstance(
        const WmiTargetConfig& target,
        const std::wstring& objectPath,
        std::wstring& output,
        std::wstring& error
    ) const override {
        ScopedMiSession miScope;
        ScopedMiOperation queryOperation;
        ScopedMiOperation deleteOperation;
        ScopedMiInstance extendedError;
        ScopedMiInstance targetInstance;

        std::wstring query;
        if (!TryBuildInstanceSelectQueryFromObjectPath(objectPath, query)) {
            error = L"WinRM native backend path delete requires object paths in the form Class.Key=Value[,Key=Value].";
            return false;
        }

        MI_Result miResult = MI_Application_Initialize(0, NULL, &extendedError.instance, &miScope.application);
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_Initialize failed.";
            return false;
        }

        miResult = MI_Application_NewDestinationOptions(&miScope.application, &miScope.destinationOptions);
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_NewDestinationOptions failed.";
            return false;
        }

        if (!target.user.empty()) {
            MI_UserCredentials creds;
            memset(&creds, 0, sizeof(creds));
            creds.authenticationType = MI_AUTH_TYPE_NEGO_WITH_CREDS;
            creds.credentials.usernamePassword.domain = target.domain.empty() ? NULL : target.domain.c_str();
            creds.credentials.usernamePassword.username = target.user.c_str();
            creds.credentials.usernamePassword.password = target.password.c_str();
            miResult = MI_DestinationOptions_AddDestinationCredentials(&miScope.destinationOptions, &creds);
            if (miResult != MI_RESULT_OK) {
                error = L"MI_DestinationOptions_AddDestinationCredentials failed.";
                return false;
            }
        }

        const MI_Char* destination = (target.node.empty() || target.node == L".") ? NULL : target.node.c_str();
        miResult = MI_Application_NewSession(
            &miScope.application,
            MI_T("WINRM"),
            destination,
            &miScope.destinationOptions,
            NULL,
            &extendedError.instance,
            &miScope.session
        );
        if (miResult != MI_RESULT_OK) {
            error = L"MI_Application_NewSession failed.";
            return false;
        }

        MI_Session_QueryInstances(
            &miScope.session,
            0,
            NULL,
            target.wmiNamespace.c_str(),
            MI_T("WQL"),
            query.c_str(),
            NULL,
            &queryOperation.operation
        );

        for (;;) {
            const MI_Instance* instance = NULL;
            MI_Boolean moreResults = MI_FALSE;
            MI_Result operationResult = MI_RESULT_OK;
            const MI_Char* errorMessage = NULL;
            const MI_Instance* completionDetails = NULL;

            miResult = MI_Operation_GetInstance(
                &queryOperation.operation,
                &instance,
                &moreResults,
                &operationResult,
                &errorMessage,
                &completionDetails
            );

            if (miResult != MI_RESULT_OK) {
                error = L"MI_Operation_GetInstance failed during target instance resolution.";
                return false;
            }

            if (instance && targetInstance.instance == NULL) {
                if (MI_Instance_Clone(instance, &targetInstance.instance) != MI_RESULT_OK || !targetInstance.instance) {
                    error = L"MI_Instance_Clone failed while preparing delete operation.";
                    return false;
                }
            }

            if (!moreResults) {
                if (operationResult != MI_RESULT_OK) {
                    error = errorMessage ? std::wstring(errorMessage) : L"MI query operation failed while resolving delete target.";
                    return false;
                }
                break;
            }
        }

        if (!targetInstance.instance) {
            error = L"Instance not found for delete operation.";
            return false;
        }

        MI_Session_DeleteInstance(
            &miScope.session,
            0,
            NULL,
            target.wmiNamespace.c_str(),
            targetInstance.instance,
            NULL,
            &deleteOperation.operation
        );

        for (;;) {
            const MI_Instance* instance = NULL;
            MI_Boolean moreResults = MI_FALSE;
            MI_Result operationResult = MI_RESULT_OK;
            const MI_Char* errorMessage = NULL;
            const MI_Instance* completionDetails = NULL;

            miResult = MI_Operation_GetInstance(
                &deleteOperation.operation,
                &instance,
                &moreResults,
                &operationResult,
                &errorMessage,
                &completionDetails
            );

            if (miResult != MI_RESULT_OK) {
                error = L"MI_Operation_GetInstance failed during delete operation.";
                return false;
            }

            if (!moreResults) {
                if (operationResult != MI_RESULT_OK) {
                    error = errorMessage ? std::wstring(errorMessage) : L"MI delete operation failed.";
                    return false;
                }
                break;
            }
        }

        output = L"Instance deleted successfully.";
        return true;
    }
};

struct Table {
    std::vector<std::wstring> headers;
    std::vector<std::vector<std::wstring>> rows;

    void Print() {
        if (headers.empty()) return;

        auto appendPadded = [](std::wstring& out, const std::wstring& text, size_t width) {
            out += text;
            if (text.size() < width) {
                out.append(width - text.size(), L' ');
            }
        };

        if (g_outputOptions.format == OutputFormat::Csv) {
            std::wstring out;
            out.reserve((headers.size() + rows.size() * (headers.size() + 1)) * 32);
            const std::wstring emptyCell;

            if (!g_outputOptions.noHeader) {
                for (size_t i = 0; i < headers.size(); ++i) {
                    if (i > 0) out.push_back(L',');
                    out += EscapeCsv(headers[i]);
                }
                out.push_back(L'\n');
            }

            for (const auto& row : rows) {
                for (size_t i = 0; i < headers.size(); ++i) {
                    if (i > 0) out.push_back(L',');
                    const std::wstring& cell = (i < row.size()) ? row[i] : emptyCell;
                    out += EscapeCsv(cell);
                }
                out.push_back(L'\n');
            }

            std::wcout << out;
            return;
        }

        if (g_outputOptions.format == OutputFormat::Json) {
            std::wstring out;
            out.reserve((headers.size() + rows.size() * (headers.size() + 1)) * 40);
            out.push_back(L'[');
            for (size_t r = 0; r < rows.size(); ++r) {
                if (r > 0) out.push_back(L',');
                out.push_back(L'{');
                for (size_t c = 0; c < headers.size(); ++c) {
                    if (c > 0) out.push_back(L',');
                    std::wstring key = EscapeJson(headers[c]);
                    std::wstring val = (c < rows[r].size()) ? EscapeJson(rows[r][c]) : L"";
                    out += L"\"";
                    out += key;
                    out += L"\":\"";
                    out += val;
                    out += L"\"";
                }
                out.push_back(L'}');
            }
            out += L"]\n";
            std::wcout << out;
            return;
        }

        std::vector<size_t> widths(headers.size());
        for (size_t i = 0; i < headers.size(); ++i) {
            widths[i] = headers[i].length();
        }

        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
                widths[i] = (std::max)(widths[i], row[i].length());
            }
        }

        std::wstring out;
        out.reserve((rows.size() + 4) * (headers.size() * 24));
        out.push_back(L'\n');

        if (!g_outputOptions.noHeader) {
            for (size_t i = 0; i < headers.size(); ++i) {
                appendPadded(out, headers[i], widths[i] + 3);
            }
            out.push_back(L'\n');

            size_t totalWidth = 0;
            for (size_t w : widths) totalWidth += w + 3;
            out.append(totalWidth, L'-');
            out.push_back(L'\n');
        }

        const std::wstring emptyCell;
        for (const auto& row : rows) {
            for (size_t i = 0; i < widths.size(); ++i) {
                const std::wstring& cell = (i < row.size()) ? row[i] : emptyCell;
                appendPadded(out, cell, widths[i] + 3);
            }
            out.push_back(L'\n');
        }

        out.push_back(L'\n');
        std::wcout << out;
    }
};

struct InstalledAppRecord {
    std::wstring name;
    std::wstring version;
    std::wstring publisher;
};

std::wstring ReadRegistryStringValueWithHive(
    IWbemServices* pSvc,
    IWbemClassObject* pRegClass,
    unsigned long hive,
    const std::wstring& subKey,
    const std::wstring& valueName
);

enum class WslProbeError {
    None,
    NoDistrosFound,
    UnsupportedTransport,
    RemoteTargetSkipped,
    AccessDenied,
    RegistryUnavailable,
    WslNotInstalled,
    PowerShellFailed,
    UnsupportedHost,
    Unknown
};

struct WslDetectionResult {
    WslProbeError status = WslProbeError::None;
    bool foundDistros = false;
    bool attemptedRegistry = false;
    bool registryAccessible = false;
    bool attemptedCli = false;
    bool cliSucceeded = false;
    bool isLocalTarget = false;
    std::vector<std::pair<std::wstring, std::wstring>> distros;
    std::vector<std::wstring> diagnostics;
    std::vector<std::wstring> errors;
};

bool ParseWslListOutput(const std::wstring& wslOutput, std::vector<std::pair<std::wstring, std::wstring>>& distrosOut, const std::wstring& sourceLabel);

bool ExecuteWMIQuery(
    const WmiTargetConfig& target,
    const std::wstring& wqlQuery,
    const std::vector<std::wstring>& properties,
    bool autoByteFormat,
    const std::wstring& wmiLocale,
    const std::vector<std::wstring>& displayHeaders,
    const WmiSecurityConfig& securityConfig
);