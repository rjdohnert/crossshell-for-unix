#include "system_security.hpp"

bool SystemSecurity::is_elevated() {
        bool elevated = false;
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            TOKEN_ELEVATION elevation;
            DWORD cbSize = sizeof(TOKEN_ELEVATION);
            if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
                elevated = (elevation.TokenIsElevated != 0);
            }
            CloseHandle(hToken);
        }
        return elevated;
    }
