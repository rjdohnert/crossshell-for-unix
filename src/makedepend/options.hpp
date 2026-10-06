#ifndef MAKEDEPEND_OPTIONS_HPP
#define MAKEDEPEND_OPTIONS_HPP

#include "makedepend.hpp"

struct MakedependOptions {
    std::string makefilePath = "Makefile";
    std::string delimiter = DEFAULT_DELIMITER;
    std::string objSuffix = ".obj";
    std::string objPrefix = "";
    bool appendOnly = false;
    bool autoDetectCL = false;
    bool autoDetectGCC = false;
    bool autoDetectClang = false;
    bool verbose = false;
    std::string msvcVer = "1930";
    std::string gccVer = "";
    std::string clangVer = "";
    std::vector<std::string> includeDirs;
    std::vector<std::pair<std::string, std::string>> defines;
    std::vector<std::string> undefines;
    std::vector<std::string> sourceFiles;
    bool showHelp = false;
    bool showVersion = false;
};

bool parse_options(int argc, char* argv[], MakedependOptions& opts);
void print_version();
void print_help();

#endif // MAKEDEPEND_OPTIONS_HPP
