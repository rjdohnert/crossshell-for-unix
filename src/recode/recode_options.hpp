#pragma once

#include "recode.hpp"

struct RecodeOptions {
    bool verbose{false};
    bool force{false};
    bool listCharsets{false};
    std::string request;
    std::vector<std::string> fileList;

    static void showHelp();

    static void listSupported();

    static RecodeOptions parse(int argc, char* argv[]);
};
