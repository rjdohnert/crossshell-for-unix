#pragma once

#include "tcsh.hpp"

struct TcshOptions {
    bool helpRequested = false;
    bool versionRequested = false;
    bool runSelfTests = false;
    bool loadRc = true;
    std::string commandString;
    std::string scriptName = "tcsh";
    std::string scriptFile;
    std::vector<std::string> scriptArgs;
};

class OptionParser {
public:
    bool Parse(int argc, char* argv[], TcshOptions& options);
};
