#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <string>
#include <vector>

struct LocateOptions {
    bool follow_symlinks = false;
    std::vector<std::string> start_paths;
    std::vector<std::string> expr_tokens;
    bool show_help = false;
    bool show_version = false;
};

class CommandLineParser {
public:
    static void printUsage();
    static void printVersion();
    static std::string canonicalizeOption(const std::string& arg);
    static LocateOptions parse(int argc, char* argv[]);
};

#endif // OPTIONS_HPP
