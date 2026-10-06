#include "json_mini_parser.hpp"

std::string JsonMiniParser::get_string(const std::string& json, const std::string& key) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return "";

        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";

        pos = json.find_first_not_of(" \t\r\n", pos + 1);
        if (pos == std::string::npos || json[pos] != '"') return "";

        size_t end_quote = json.find('"', pos + 1);
        if (end_quote == std::string::npos) return "";

        return json.substr(pos + 1, end_quote - pos - 1);
    }

double JsonMiniParser::get_double(const std::string& json, const std::string& key, double default_val ) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return default_val;

        pos = json.find(':', pos);
        if (pos == std::string::npos) return default_val;

        pos = json.find_first_not_of(" \t\r\n", pos + 1);
        if (pos == std::string::npos) return default_val;

        size_t end_val = json.find_first_of(",}\r\n ", pos);
        std::string num_str = json.substr(pos, end_val - pos);
        try {
            return std::stod(num_str);
        } catch (...) {
            return default_val;
        }
    }
