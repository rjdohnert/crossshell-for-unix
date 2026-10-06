#pragma once

#include "nt_api_engine.hpp"
#include "option_parser.hpp"
#include "pdh_fallback_engine.hpp"
#include "vmstat.hpp"

class VmstatApplication {
private:
    OptionParser m_parser;
    NtApiEngine m_ntEngine;
    PdhFallbackEngine m_pdhEngine;

public:
    int Run(int argc, wchar_t* argv[]);
};
