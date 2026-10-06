#pragma once

#include "unxz_backend_config.hpp"
#include "unxz.hpp"

class UnxzProcessRunner {
private:
    static std::wstring quoteArgument(const std::wstring& arg);

public:
    static int execute(const UnxzBackendConfig& backend, const std::vector<std::wstring>& userArgs);
};
