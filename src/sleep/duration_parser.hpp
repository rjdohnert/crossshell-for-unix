#ifndef DURATION_PARSER_HPP
#define DURATION_PARSER_HPP

#include "sleep.hpp"

class DurationParser {
public:
    static std::string wideToUtf8(const std::wstring& text);
    static bool parseSleepArgument(const std::wstring& arg, double& totalMs);
};

#endif // DURATION_PARSER_HPP
