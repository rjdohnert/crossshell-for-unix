#ifndef NEWSHELL_ENGINE_HPP
#define NEWSHELL_ENGINE_HPP

#include "newshell.hpp"

class TerminalLauncher {
public:
    static bool IsWindowsTerminalAvailable();
    static bool Launch(const std::wstring& targetExe,
                       const std::wstring& targetArgs,
                       const std::wstring& workingDir,
                       bool runAsAdmin);
};

#endif // NEWSHELL_ENGINE_HPP
