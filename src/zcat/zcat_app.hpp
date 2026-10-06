#pragma once

#include "gzip_streamer.hpp"
#include "option_parser.hpp"
#include "process_runner.hpp"
#include "zcat.hpp"

class ZcatApplication {
private:
    OptionParser m_parser;
    ProcessRunner m_runner;
    GzipStreamer m_streamer;

public:
    ZcatApplication();

    int Run(int argc, wchar_t* argv[]);
};
