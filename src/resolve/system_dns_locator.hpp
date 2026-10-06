#pragma once

#include "resolve.hpp"

class SystemDnsLocator {
public:
    static std::string GetPrimaryDnsServer();
};
