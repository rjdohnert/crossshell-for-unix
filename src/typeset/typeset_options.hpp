#pragma once

#include "typeset.hpp"
#include "variable_attributes.hpp"

struct TypesetOptions {
    VariableAttributes attributes;
    bool optPrint = false;
    bool showHelp = false;
    bool showVersion = false;
    OutputFormat format = OutputFormat::Default;
    std::wstring pipeCommand;
    std::vector<std::wstring> targets;
};
