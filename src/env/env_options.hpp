#ifndef ENV_OPTIONS_HPP
#define ENV_OPTIONS_HPP

#include <string>
#include <utility>
#include <vector>

enum class OutputFormat {
    Plain,
    Json,
    Csv,
    Table
};

struct ProgramOptions {
    bool ignoreEnvironment = false;
    bool nullDelimiter = false;
    bool verbose = false;
    bool showHelp = false;
    bool showVersion = false;
    std::wstring changeDir;
    std::wstring pipeCommand;
    OutputFormat outputFormat = OutputFormat::Plain;
    std::vector<std::wstring> unsetVariables;
    std::vector<std::pair<std::wstring, std::wstring>> setVariables;
    std::vector<std::wstring> commandAndArgs;
};

struct ParseResult {
    bool success = false;
    ProgramOptions options;
    std::wstring error;
};

#endif
