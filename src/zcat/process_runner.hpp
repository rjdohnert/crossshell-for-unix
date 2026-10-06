#pragma once

#include "zcat.hpp"

class ProcessRunner {
public:
    int RunPowerShell(const std::wstring& script) const;
};
