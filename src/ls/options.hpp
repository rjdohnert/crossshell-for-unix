#ifndef OPTIONS_HPP
#define OPTIONS_HPP

#include <string>
#include <vector>

class ListingOptions {
public:
    bool all = false;          // -a: List all entries (. and ..)
    bool almostAll = false;    // -A: List entries except . and ..
    bool longFormat = false;   // -l: Detailed listing with Windows modes
    bool typeIndicator = false;// -F: Append /, *, @
    bool recursive = false;    // -R: Recursive subtree listing
    bool reverseSort = false;  // -r: Reverse sort order
    bool sortByTime = false;   // -t: Sort by modification time
    bool sortBySize = false;   // -S: Sort by file size
    bool singleColumn = false; // -1: Single column output
    bool color = true;         // ANSI TrueColor support
    std::string outputFormat;  // empty, json, csv, or table
    std::vector<std::string> targets;
};

class HelpFormatter {
public:
    static void printHelp();
    static void printVersion();
};

class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], ListingOptions& options);
};

#endif // OPTIONS_HPP
