#include "vms_status.hpp"

VmsStatusReporter::VmsStatusReporter(int fmt, std::wstring pipeCmd)
    : outputFormat(fmt), pipeCommand(std::move(pipeCmd)) {}

void VmsStatusReporter::printStatus(wchar_t severity, std::wstring_view facility, std::wstring_view ident, std::wstring_view text) const {
    std::wstring line = L"%" + std::wstring(facility) + L"-" + severity + L"-" + std::wstring(ident) + L", " + std::wstring(text);
    if (outputFormat == 1) std::wcout << L"{\"status\":\"" << line << L"\"}\n";
    else if (outputFormat == 2) std::wcout << L"\"status\"\n\"" << line << L"\"\n";
    else if (outputFormat == 3) std::wcout << L"STATUS\n" << line << L"\n";
    else std::wcout << line << L"\n";
}

std::wstring VmsStatusReporter::toUpper(std::wstring_view str) {
    std::wstring result(str);
    std::transform(result.begin(), result.end(), result.begin(), ::towupper);
    return result;
}
