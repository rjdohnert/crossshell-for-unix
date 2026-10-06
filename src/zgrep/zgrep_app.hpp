#pragma once

#include "grep_backend_locator.hpp"
#include "gzip_decompressor.hpp"
#include "option_parser.hpp"
#include "process_runner.hpp"
#include "zgrep.hpp"

class ZgrepApplication {
private:
    OptionParser m_parser;
    ProcessRunner m_runner;
    GzipDecompressor m_decompressor;
    GrepBackendLocator m_backendLocator;

public:
    ZgrepApplication();

    int Run(int argc, wchar_t* argv[]);
};
