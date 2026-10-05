#pragma once

#include <string>
#include <vector>

class CatOptions {
public:
    bool numberLines = false;
    bool numberNonBlank = false;
    bool showEnds = false;
    bool squeezeBlanks = false;
    bool silent = false;
    bool showTabs = false;
    bool unbuffered = false;
    bool showNonPrinting = false;
    bool outputJson = false;

    std::vector<std::string> files;

    void normalizeDependencies();
    bool requiresFormatting() const;
};

class HelpFormatter {
public:
    static void printHelp();
};

class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], CatOptions& options);
};
