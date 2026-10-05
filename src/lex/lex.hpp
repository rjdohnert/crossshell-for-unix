#ifndef LEX_HPP
#define LEX_HPP

#include <string>
#include <vector>

struct FlexOptions {
    bool generateCpp = false;             // -+, --c++
    bool stdoutMode = false;              // -t, --stdout
    bool caseInsensitive = false;         // -i, --case-insensitive
    bool suppressDefault = false;         // -s, --nodefault
    bool debugMode = false;               // -d, --debug
    bool verbose = false;                 // -v, --verbose
    bool noyywrap = true;                 // %option noyywrap
    bool showHelp = false;                // -?, --help
    bool showVersion = false;             // -V, --version
    std::string prefix = "yy";            // -P, --prefix=PREFIX
    std::string outputFile;              // -o, --outfile=FILE
    std::string headerFile;              // --header-file=FILE
    std::string inputFilePath;           // Input .l/.flex file or "-" for stdin
};

struct LexRule {
    std::string pattern;
    std::string action;
    int lineNumber = 0;
};

#endif // LEX_HPP
