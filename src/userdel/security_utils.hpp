#pragma once

#include "userdel.hpp"

class SecurityUtils {
public:
    static std::optional<std::wstring> GetUserSidString(const std::string& username);
};
