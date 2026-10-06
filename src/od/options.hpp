#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include "od.hpp"

class OdOptions {
public:
    char addressRadix{'o'}; // 'o', 'd', 'x', 'n'
    uint64_t skipBytes{0};
    uint64_t limitBytes{UINT64_MAX};
    bool outputDuplicates{false}; // -v
    size_t bytesPerLine{16};      // -w
    std::vector<FormatSpec> formats;
    std::vector<std::string> filenames;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* prog);
    static void printVersion();
    static uint64_t parseByteCount(const std::string& str);
    static bool parseTypeString(const std::string& typeStr, std::vector<FormatSpec>& formats);
    static bool parse(int argc, char* argv[], OdOptions& opts);
};

#endif // OPTIONS_HPP
