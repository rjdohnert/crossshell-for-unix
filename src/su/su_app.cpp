#include "execute_in_same_terminal.hpp"
#include "parse_user_and_domain.hpp"
#include "read_password.hpp"
#include "shell_command.hpp"
#include "su_app.hpp"
#include "su_help.hpp"

int runSu(int argc, wchar_t* argv[]) {
    std::wstring targetUser = L"Administrator";
    std::wstring customCommand = L"";
    bool loginShell = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"--help" || arg == L"-h" || arg == L"-?" || arg == L"/?") {
            ShowHelp();
            return 0;
        } else if (arg == L"-" || arg == L"-l" || arg == L"--login") {
            loginShell = true;
        } else if ((arg == L"-c" || arg == L"--command") && i + 1 < argc) {
            customCommand = argv[++i];
        } else if (arg[0] != L'-') {
            targetUser = arg;
        }
    }

    std::wstring username, domain;
    ParseUserAndDomain(targetUser, username, domain);

    // Prompt for target account password
    std::wstring prompt = L"Password for " + targetUser + L": ";
    std::wstring password = ReadPassword(prompt);

    // Determine target execution binary inside Windows Terminal.
    std::wstring shellCommand = BuildShellCommand(customCommand, loginShell);
    std::wstring commandLine = L"wt.exe new-tab " + shellCommand;

    DWORD result = ExecuteInSameTerminal(username, domain, password, commandLine);
    return (result == 0) ? 0 : 1;
}
