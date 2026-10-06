#pragma once

#include "ulimit.hpp"

class PrivilegeEscalator {
public:
    static bool EnableDebugPrivilege();
};
