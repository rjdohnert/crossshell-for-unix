#pragma once

#include "stat.hpp"

class SecurityInspector {
public:
    static std::wstring getFileOwner(const std::wstring& path);
};
