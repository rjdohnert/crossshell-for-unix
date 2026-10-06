#pragma once

#include "tr.hpp"

class OutputFormatter {
public:
    static std::string jsonQuote(const std::string& value);

    static std::string csvQuote(const std::string& value);

    static int output(const std::string& content, int format, const std::string& pipeCommand);
};
