#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct Options {
    std::vector<uint8_t> searchPattern;
    std::vector<uint8_t> replacePattern;
    bool hasSearch = false;
    bool hasReplace = false;
    bool caseInsensitive = false;
    bool countOnly = false;
    bool listFilesOnly = false;
    bool verbose = false;
    bool showOffsets = false;
    bool createBackup = false;
    bool filterPipeMode = false;
    bool displayHex = false;
    bool forceOverwrite = false;
    bool showHelp = false;
    bool showVersion = false;
    std::string backupExtension = ".bak";
    std::vector<std::filesystem::path> inputFiles;
    std::filesystem::path outputFile;
};
