#ifndef SEQ_OPTIONS_HPP
#define SEQ_OPTIONS_HPP

#include "seq.hpp"

class SeqOptions {
public:
    std::string separator{"\n"};
    std::string customFormat{""};
    bool equalWidth{false};
    std::string startStr{"1"};
    std::string incrementStr{"1"};
    std::string lastStr{""};

    static void printUsage(const char* progName);
    static void printVersion();
    static bool parse(int argc, char* argv[], SeqOptions& outOpts);
};

#endif // SEQ_OPTIONS_HPP
