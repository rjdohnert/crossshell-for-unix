/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "engine.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>

bool SessionManager::TerminateSession(bool force) {
    UINT flags = EWX_LOGOFF;
    if (force) {
        flags |= EWX_FORCE;
    }

    if (ExitWindowsEx(flags, 0)) {
        return true;
    }

    DWORD err = GetLastError();
    std::cerr << "logout: ExitWindowsEx failed with error code: " << err << std::endl;
    return false;
}
