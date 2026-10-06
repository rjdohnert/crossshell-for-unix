#include "admin_check.hpp"
#include "pipe_client_security.hpp"

bool IsPipeClientPrivileged(HANDLE hPipe) {
    if (!ImpersonateNamedPipeClient(hPipe)) return false;

    HANDLE token = NULL;
    bool allowed = false;
    if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
        allowed = IsTokenElevatedOrAdmin(token);
        CloseHandle(token);
    }
    RevertToSelf();
    return allowed;
}
