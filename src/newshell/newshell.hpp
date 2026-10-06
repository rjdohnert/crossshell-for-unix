/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * WinNewshell - Terminal & Console Host Launcher for Windows
 */

#ifndef NEWSHELL_HPP
#define NEWSHELL_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>

struct NewshellOptions {
    bool runAsAdmin{false};
    bool forceConhost{false};
    std::wstring workingDir;
    std::wstring profileName;
    std::wstring customCommand;
    bool showHelp{false};
    bool showVersion{false};
};

#endif // NEWSHELL_HPP
