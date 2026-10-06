#pragma once

#include "unxz.hpp"

struct UnxzBackendConfig {
    std::wstring applicationPath;
    std::vector<std::wstring> fixedArgs;
};
