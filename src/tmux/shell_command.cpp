#include "shell_command.hpp"
#include "string_encoding.hpp"

std::wstring DetectDefaultShell() {
    return L"powershell.exe";
}

std::string ShellTabName(const std::wstring& shellCmd) {
    std::string shell = WStringToString(shellCmd);
    size_t firstArg = shell.find(' ');
    std::string exe = firstArg == std::string::npos ? shell : shell.substr(0, firstArg);
    exe.erase(std::remove(exe.begin(), exe.end(), '"'), exe.end());
    size_t slash = exe.find_last_of("\\/");
    if (slash != std::string::npos) exe = exe.substr(slash + 1);
    size_t dot = exe.find_last_of('.');
    if (dot != std::string::npos) exe = exe.substr(0, dot);
    return exe.empty() ? "shell" : exe;
}
