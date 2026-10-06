#ifndef NUMBER_PARSER_HPP
#define NUMBER_PARSER_HPP

#include "seq.hpp"

class NumberParser {
public:
    static std::string unescape(const std::string& input);
    static NumberInfo parse(const std::string& str);
};

#endif // NUMBER_PARSER_HPP
