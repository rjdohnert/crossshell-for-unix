#pragma once

#include "wall.hpp"

class SystemInfoProvider {
public:
    static std::wstring utf8ToWide(const std::string& str);

    static std::string getCurrentUserName();

    static std::string getCurrentHostName();

    static std::string getCurrentTimestamp();
};
