#pragma once

#include "toolchain_locator.hpp"
#include "vcc.hpp"

struct BuildOptions {
    bool showHelp = false;
    bool showVersion = false;
    bool compileOnly = false;
    bool sharedLibrary = false;
    TargetArch targetArch = TargetArch::X64;
    std::string programName;
    std::vector<std::string> compilerArgs;
    std::vector<std::string> linkerArgs;
    std::vector<std::string> sourceFiles;
};
