#pragma once

#include "backend_config.hpp"
#include "xz.hpp"

class XzBackendFinder {
private:
    static std::wstring toLower(const std::wstring& s);

    static std::wstring normalizePath(const std::wstring& p);

    static std::wstring findInPath(const std::wstring& exe);

    static std::wstring selfExePath();

public:
    static BackendConfig discoverBackend();
};
