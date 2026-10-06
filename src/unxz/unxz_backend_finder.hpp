#pragma once

#include "unxz_backend_config.hpp"
#include "unxz.hpp"

class UnxzBackendFinder {
private:
    static std::wstring toLower(const std::wstring& s);

    static std::wstring normalizePath(const std::wstring& p);

    static std::wstring findInPath(const std::wstring& exe);

    static std::wstring selfExePath();

public:
    static UnxzBackendConfig discoverBackend();
};
