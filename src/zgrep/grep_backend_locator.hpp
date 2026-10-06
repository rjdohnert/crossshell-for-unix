#pragma once

#include "zgrep.hpp"

class GrepBackendLocator {
public:
    static std::wstring GetModuleDirectory();

    std::wstring Resolve() const;
};
