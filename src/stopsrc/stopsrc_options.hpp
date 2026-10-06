#pragma once

#include "output_formatter.hpp"
#include "stopsrc.hpp"

struct StopsrcOptions {
    std::wstring service;
    DWORD timeoutMs = 30000;
    bool help = false;
    bool version = false;
    OutputFormat format = OutputFormat::Default;
    std::wstring pipe;
};
