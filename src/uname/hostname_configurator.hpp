#pragma once

#include "uname.hpp"

class HostnameConfigurator {
public:
    static bool setHostname(const std::wstring& newName);
};
