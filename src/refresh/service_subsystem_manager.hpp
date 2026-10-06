#pragma once

#include "refresh_options.hpp"
#include "refresh.hpp"

class ServiceSubsystemManager {
public:
    static bool WaitForState(SC_HANDLE svc, DWORD wantedState, DWORD timeoutMs);

    static bool StopThenStart(SC_HANDLE svc, DWORD timeoutMs, const std::wstring& name);

    static int RefreshService(const RefreshOptions& opt);
};
