#pragma once

#include "refresh.hpp"

class RefreshOptions {
public:
    std::wstring service;
    DWORD timeoutMs = 30000;
    bool restartFallback = false;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]);

    void PrintHelp() const;

    void PrintVersion() const;
};
