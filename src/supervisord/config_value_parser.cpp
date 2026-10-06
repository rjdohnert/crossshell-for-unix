#include "config_value_parser.hpp"

bool TryParseInt(const std::wstring& input, int& out) {
    if (input.empty()) return false;
    try {
        size_t idx = 0;
        int value = std::stoi(input, &idx);
        if (idx != input.size()) return false;
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool TryParseUnsignedLongLong(const std::wstring& input, unsigned long long& out) {
    if (input.empty()) return false;
    try {
        size_t idx = 0;
        unsigned long long value = std::stoull(input, &idx);
        if (idx != input.size()) return false;
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool TryParseDword(const std::wstring& input, DWORD& out) {
    unsigned long long value = 0;
    if (!TryParseUnsignedLongLong(input, value) || value > 0xFFFFFFFFull) return false;
    out = static_cast<DWORD>(value);
    return true;
}

std::wstring TrimConfigToken(std::wstring s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](wchar_t ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](wchar_t ch) { return !std::isspace(ch); }).base(), s.end());
    if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}
