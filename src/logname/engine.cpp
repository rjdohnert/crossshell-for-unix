/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 */

#include "engine.hpp"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <lmcons.h>
#include <cstdlib>

std::string LoginNameProvider::GetLoginName() {
    const char* envLogname = std::getenv("LOGNAME");
    if (envLogname != nullptr && envLogname[0] != '\0') {
        return std::string(envLogname);
    }

    const char* envUser = std::getenv("USER");
    if (envUser != nullptr && envUser[0] != '\0') {
        return std::string(envUser);
    }

    const char* envUsername = std::getenv("USERNAME");
    if (envUsername != nullptr && envUsername[0] != '\0') {
        return std::string(envUsername);
    }

    wchar_t username[UNLEN + 1] = { 0 };
    DWORD size = UNLEN + 1;
    if (GetUserNameW(username, &size)) {
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, username, -1, nullptr, 0, nullptr, nullptr);
        if (sizeNeeded > 1) {
            std::string str(static_cast<size_t>(sizeNeeded - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, username, -1, &str[0], sizeNeeded, nullptr, nullptr);
            return str;
        }
    }

    return "";
}
