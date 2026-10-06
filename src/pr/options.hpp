#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "pr.hpp"

class PrOptions {
public:
    int pageLength{66};
    int width{72};
    bool omitHeader{false};
    std::wstring header;
    int outputFormat{0};
    std::wstring pipeCommand;
    std::vector<std::wstring> files;

    static void printHelp();
    static void printVersion();
    static bool parse(int argc, wchar_t* argv[], PrOptions& opts);
};

#endif // OPTIONS_HPP
