#pragma once

#include "wall.hpp"

class WallConfig {
public:
    static constexpr size_t MAX_MSG_LIMIT = 500;
    static constexpr const char* VERSION = "1.0.0";

    bool showHelp{false};
    bool showVersion{false};
    bool noBanner{false};
    bool guiPopup{false};
    bool useConhost{false};
    std::string filePath{""};
    std::string inlineMessage{""};
    std::string serverName{""};
    int outputFormat{0}; // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;

    static void printHelp(const char* exeName);

    static void printVersion();

    static bool parse(int argc, char* argv[], WallConfig& cfg);
};
