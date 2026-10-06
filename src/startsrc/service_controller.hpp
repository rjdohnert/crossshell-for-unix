#pragma once

#include "startsrc.hpp"

class ServiceController {
public:
    static bool WaitForState(SC_HANDLE svc, DWORD wantedState, DWORD timeoutMs);
};
