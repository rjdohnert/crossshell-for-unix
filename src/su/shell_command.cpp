#include "shell_command.hpp"

std::wstring QuoteForCommandLine(const std::wstring& value) {
    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'\"') {
            quoted += L"\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += L"\"";
    return quoted;
}

std::wstring BuildShellCommand(const std::wstring& customCommand, bool loginShell) {
    if (!customCommand.empty()) {
        std::wstring shell = loginShell ? L"ksh.exe -l -c " : L"ksh.exe -c ";
        return shell + QuoteForCommandLine(customCommand);
    }
    return loginShell ? L"ksh.exe -l" : L"ksh.exe";
}
