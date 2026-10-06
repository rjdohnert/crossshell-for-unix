#pragma once

#include "reboot.hpp"

class PrivilegeManager {
public:
    static bool EnableShutdownPrivilege();

    static bool IsRunningAsAdmin();
};
