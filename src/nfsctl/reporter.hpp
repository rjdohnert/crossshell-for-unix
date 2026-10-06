#ifndef NFSCTL_REPORTER_HPP
#define NFSCTL_REPORTER_HPP

#include "nfsctl.hpp"

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);

class NfsOutputFormatter {
public:
    static bool IsCancelled();
    static bool IsDeadlineExceeded(ULONGLONG startTick, const GlobalOptions& options);
    static DWORD CheckOperationState(ULONGLONG startTick, const GlobalOptions& options);

    static void LogInfo(const GlobalOptions& options, const wchar_t* fmt, ...);
    static void LogVerbose(const GlobalOptions& options, const wchar_t* fmt, ...);
    static void LogTrace(const GlobalOptions& options, const wchar_t* fmt, ...);

    static std::wstring ToLowerWide(std::wstring input);
    static bool ContainsIcase(const std::wstring& haystack, const std::wstring& needle);
    static std::wstring JsonEscape(const std::wstring& input);
    static std::wstring RegistryTypeLabel(DWORD type);
    static std::wstring RegistryStringFromData(const BYTE* data, DWORD size);
    static std::wstring RegistryBinaryToHex(const BYTE* data, DWORD size);
    static void AppendRegistryValueJson(std::wstringstream& out, DWORD type, const BYTE* data, DWORD size);
    static DWORD BuildRegistryValuesJson(HKEY hKeyRoot, const wchar_t* subKey, std::wstring& outJson, DWORD& valueCount);

    static std::wstring Win32ErrorMessage(DWORD code);
    static void PrintWin32Error(const wchar_t* context, DWORD code);
    static int NormalizeExitCode(DWORD code);

    static std::wstring AddressFamilyLabel(int addressFamily);
    static std::wstring NfsProbeModeLabel(NfsProbeMode mode);
    static void PrintDelimitedCell(const std::wstring& value, OutputMode mode);
    static void PrintDelimitedRow(const std::vector<std::wstring>& cells, OutputMode mode);
    static void PrintJsonEnvelope(const std::wstring& command, DWORD code, const std::wstring& dataJson, const std::wstring& errorMessage = L"");
};

#endif // NFSCTL_REPORTER_HPP
