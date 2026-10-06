#ifndef MKBOOT_OPTIONS_HPP
#define MKBOOT_OPTIONS_HPP

#include "mkboot.hpp"

struct ConfigOptions {
    fs::path sourcePath;
    fs::path outputIsoPath;
    fs::path bootFilePath;
    fs::path mountPath;
    std::vector<fs::path> driverPaths;
    std::vector<std::string> execCommands;
    std::string architecture = "x64";
    std::string volumeLabel = "WINPE_BOOT";
    bool ignoreDriverErrors = false;
    bool force = false;
    bool verbose = false;
    bool showHelp = false;
};

void PrintHelp(const char* exeName);
bool ParseCommandLine(int argc, char* argv[], ConfigOptions& config);

#endif // MKBOOT_OPTIONS_HPP
