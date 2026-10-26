#include "privilege_check.h"

bool Yum_IsElevated(void) {
    HANDLE token = NULL;
    TOKEN_ELEVATION elevation = {0};
    DWORD bytes = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    BOOL success = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &bytes);
    CloseHandle(token);
    return success && elevation.TokenIsElevated != 0;
}
