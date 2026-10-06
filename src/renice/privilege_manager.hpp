#pragma once

#include "renice.hpp"

class PrivilegeManager {
public:
    static bool enableDebugPrivilege() noexcept;
};
