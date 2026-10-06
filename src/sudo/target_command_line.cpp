#include "target_command_line.hpp"

std::wstring GetTargetCommandLine() {
    std::wstring cmd = GetCommandLineW();
    bool inQuote = false;
    size_t i = 0;

    // Skip executable name
    while (i < cmd.length()) {
        if (cmd[i] == L'"') inQuote = !inQuote;
        else if (cmd[i] == L' ' && !inQuote) break;
        i++;
    }
    // Skip trailing spaces
    while (i < cmd.length() && cmd[i] == L' ') i++;

    return (i < cmd.length()) ? cmd.substr(i) : L"";
}
