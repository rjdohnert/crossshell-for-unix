#pragma once

#include "gzip_types.hpp"

class GzipCli {
public:
    static bool parse(int argc, wchar_t* argv[], GzipOptions& options,
                      std::wstring& error);
    static void printHelp();
    static void printVersion();
};
