#include "type_parser.hpp"

long long TypeParser::ParseLong(const std::wstring& str) {
        size_t idx = 0;
        long long val = 0;
        std::string narrow(str.length(), '\0');
        for (size_t i = 0; i < str.length(); ++i) narrow[i] = static_cast<char>(str[i]);
        try {
            val = std::stoll(str, &idx);
        } catch (...) {
            throw std::runtime_error("integer expression expected: " + narrow);
        }
        if (idx != str.length()) {
            throw std::runtime_error("integer expression expected: " + narrow);
        }
        return val;
    }
