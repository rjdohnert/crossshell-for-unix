#pragma once

#include "backend_config.hpp"
#include "xz.hpp"

class XzProcessRunner {
private:
    static std::wstring quoteArgument(const std::wstring& arg);

public:
    static int execute(const BackendConfig& backend, const std::vector<std::wstring>& userArgs);
};
