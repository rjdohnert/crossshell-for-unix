#pragma once

#include "xz.hpp"

struct BackendConfig {
    std::wstring applicationPath;
    std::vector<std::wstring> fixedArgs;
};
