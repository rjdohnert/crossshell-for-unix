#pragma once

#include "zgrep.hpp"

class ProcessRunner {
public:
    int Run(const wchar_t* app, const std::vector<std::wstring>& args, HANDLE output = INVALID_HANDLE_VALUE) const;
};
