#pragma once

#include "traceroute.hpp"

class NetworkUtils {
public:
    static bool IsUserAdmin();

    static WORD CalculateChecksum(WORD* buffer, int size);

    static std::string ResolveHostname(IN_ADDR addr, bool numericMode);
};
