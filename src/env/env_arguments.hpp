#ifndef ENV_ARGUMENTS_HPP
#define ENV_ARGUMENTS_HPP

#include "env_options.hpp"

#include <string>
#include <vector>

class ArgumentTokenizer {
public:
    static bool SplitString(const std::wstring& value,
                            std::vector<std::wstring>& tokens,
                            std::wstring& error);
};

class CommandLineParser {
public:
    static ParseResult Parse(int argc, wchar_t* argv[]);

private:
    static bool ParseTokens(const std::vector<std::wstring>& args,
                            ProgramOptions& options, std::wstring& error,
                            bool& formatWasSet, unsigned int splitDepth);
};

#endif
