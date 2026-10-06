#include "privilege_manager.hpp"
#include "scoped_handle.hpp"

bool PrivilegeManager::enableDebugPrivilege() noexcept {
        ScopedHandle hToken;
        HANDLE rawToken = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &rawToken)) {
            return false;
        }
        hToken.reset(rawToken);

        LUID luid;
        if (!::LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            return false;
        }

        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        return ::AdjustTokenPrivileges(hToken.get(), FALSE, &tp, sizeof(tp), nullptr, nullptr) &&
               (::GetLastError() == ERROR_SUCCESS);
    }
