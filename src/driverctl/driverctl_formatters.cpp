/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "driverctl_formatters.hpp"

std::string JoinList(const std::vector<std::string>& items) {
    std::string result;
    for (const auto& item : items) {
        if (!result.empty()) result += ", ";
        result += item;
    }
    return result;
}

std::string EscapeJSON(const std::string& value) {
    static const char hex[] = "0123456789abcdef";
    std::string escaped;
    escaped.reserve(value.size());
    for (unsigned char character : value) {
        switch (character) {
            case '\\': escaped += "\\\\"; break;
            case '"':  escaped += "\\\""; break;
            case '\b': escaped += "\\b"; break;
            case '\f': escaped += "\\f"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:
                if (character < 0x20) {
                    escaped += "\\u00";
                    escaped += hex[character >> 4];
                    escaped += hex[character & 0x0f];
                } else {
                    escaped += static_cast<char>(character);
                }
        }
    }
    return escaped;
}

std::string EscapeCSV(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 3);
    escaped += '"';
    size_t firstVisible = value.find_first_not_of(" \t\r\n\v\f");
    if (firstVisible != std::string::npos &&
        (value[firstVisible] == '=' || value[firstVisible] == '+' ||
         value[firstVisible] == '-' || value[firstVisible] == '@')) {
        escaped += '\'';
    }
    for (char character : value) {
        if (character == '"') escaped += "\"\"";
        else escaped += character;
    }
    escaped += '"';
    return escaped;
}
