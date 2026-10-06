#pragma once

#include "whereami.hpp"

class JsonMiniParser {
public:
    static std::string get_string(const std::string& json, const std::string& key);

    static double get_double(const std::string& json, const std::string& key, double default_val = 0.0);
};
