#pragma once

#include "stopsrc.hpp"

class ServiceController {
public:
    static bool WaitForState(SC_HANDLE svc, DWORD wantedState, DWORD timeoutMs);
};
