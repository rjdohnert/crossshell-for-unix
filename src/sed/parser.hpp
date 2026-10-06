#pragma once

#include "sed.hpp"

class MiniJsonParser {
    std::string src;
    size_t pos = 0;

    void skip_ws();
    char peek();
    char get();
    std::string parse_string();
    Value parse_number();

public:
    explicit MiniJsonParser(std::string s);
    Value parse_value();
};

class ScriptParser {
    static std::string trim(const std::string& s);
    static std::vector<std::string> split_statements(const std::string& script_text);

public:
    [[nodiscard]] std::vector<SedCommand> parse(const std::string& script_text) const;
};
