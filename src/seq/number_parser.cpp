#include "number_parser.hpp"

std::string NumberParser::unescape(const std::string& input) {
    std::string res;
    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == '\\' && i + 1 < input.length()) {
            switch (input[i + 1]) {
                case 'n': res += '\n'; ++i; break;
                case 't': res += '\t'; ++i; break;
                case 'r': res += '\r'; ++i; break;
                case '\\': res += '\\'; ++i; break;
                default: res += input[i]; break;
            }
        } else {
            res += input[i];
        }
    }
    return res;
}

NumberInfo NumberParser::parse(const std::string& str) {
    NumberInfo info;
    info.raw = str;
    info.value = std::stod(str);

    size_t start = (str[0] == '-' || str[0] == '+') ? 1 : 0;
    size_t dotPos = str.find('.', start);

    if (dotPos == std::string::npos) {
        info.intWidth = static_cast<int>(str.length() - start);
        info.fracWidth = 0;
    } else {
        info.intWidth = static_cast<int>(dotPos - start);
        info.fracWidth = static_cast<int>(str.length() - dotPos - 1);
    }
    return info;
}
