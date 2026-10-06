#pragma once

#include "mod_options.hpp"
#include "usermod.hpp"

class WindowsAccountModifier {
public:
    static bool ModifyAccount(const ModOptions& opt, std::string& errorMessage);

private:
    static bool UpdateGroups(const ModOptions& opt, std::string& errorMessage);

    static std::string FormatNetError(NET_API_STATUS status);
};
