#pragma once

#include "xargs.hpp"

class CommandLineEscaper {
public:
    static std::string escape(const std::string& arg);

    static std::string replaceAll(std::string str, const std::string& from, const std::string& to);

    static std::string build(const std::vector<std::string>& args);
};
