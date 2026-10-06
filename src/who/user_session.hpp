#pragma once

#include "who.hpp"

struct UserSession {
    std::wstring username;
    std::wstring domain;
    std::wstring line;
    std::wstring clientName;
    std::wstring logonTime;
    bool isActive = false;
};
