#ifndef SEQUENCE_OPTIONS_HPP
#define SEQUENCE_OPTIONS_HPP

#include "sequence.hpp"

class SequenceOptions {
public:
    KeyFlags globalFlags;
    std::vector<KeyDefinition> keys;
    char delimiter{'\0'}; // '\0' = whitespace transition
    char lineTerminator{'\n'};
    bool unique{false};
    bool stable{true};
    bool checkOnly{false};
    bool checkSilent{false};
    std::string outputFile{""};
    std::vector<std::string> inputFiles;

    static void showHelp();
    static void showVersion();
    static bool parse(int argc, char* argv[], SequenceOptions& opt);
};

#endif // SEQUENCE_OPTIONS_HPP
