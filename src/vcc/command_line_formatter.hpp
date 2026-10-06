#pragma once

#include "vcc.hpp"

class CommandLineFormatter {
public:
    static std::string Quote(const std::string& arg);

    static std::string FormatArgs(const std::vector<std::string>& args);
};
