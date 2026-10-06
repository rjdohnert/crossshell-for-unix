#pragma once

#include "watch_options.hpp"
#include "watch.hpp"

class WatchConsoleReporter {
public:
    static void ClearScreen();

    static void PrintHeader(const WatchOptions& options, const std::wstring& displayCommand);

    static void PrintUsage(const wchar_t* progName);

    static void PrintVersion();
};
