#pragma once

#include "uptime.hpp"

class UptimeCalculator {
public:
    static std::string getUptimeString();

    static std::string getPrettyUptimeString();

    static std::string getBootTimeString();

    static std::string getCurrentTimeFormatted();
};
