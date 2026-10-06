#pragma once

#include "printf.hpp"

class EscapeSequenceParser {
public:
    static char ParseEscapeSequence(const std::string& str, size_t& i);
};

class TypeCoercer {
public:
    static long long ParseIntArg(const std::string& str);
    static double ParseDoubleArg(const std::string& str);
    static time_t ParseTimeArg(const std::string& str);
};

class ArgumentQuoter {
public:
    static std::string QuoteArg(const std::string& arg);
};

class FormatEngine {
public:
    static bool ProcessPercentB(const std::string& arg, std::ostream& out, bool& halt_output);
    static void ProcessTimeFormat(const std::string& timeFmt, const std::string& arg_str, std::ostream& out);
    static void ProcessSpecifier(const std::string& fmt, size_t& i,
                                 const std::vector<std::string>& args, size_t& arg_index,
                                 std::ostream& out, bool& halt_output);
    static void ExecuteFormatLoop(const std::string& format, const std::vector<std::string>& args, std::ostream& out);
};
