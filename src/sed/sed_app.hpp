#pragma once

#include "options.hpp"
#include "engine.hpp"

class SedApplication {
private:
    OptionParser m_parser;
    ScriptParser m_scriptParser;

public:
    int Run(int argc, char* argv[]);
};
