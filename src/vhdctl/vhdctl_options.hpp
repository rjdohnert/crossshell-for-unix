#pragma once

#include "vhdctl.hpp"

class VhdctlOptions {
public:
    std::wstring command;
    std::wstring filePath;
    std::wstring sizeStr;
    bool isFixed = false;
    bool readOnly = false;
    OutputFormat outputFormat = OutputFormat::Human;
    std::wstring pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, wchar_t* argv[]);
};
