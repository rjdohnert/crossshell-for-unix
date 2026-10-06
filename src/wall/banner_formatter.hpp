#pragma once

#include "wall.hpp"

class BannerFormatter {
public:
    static std::string format(const std::string& user, const std::string& host, const std::string& timeStr);
};
