#pragma once

#include "ts.hpp"

class StructuredReporter {
public:
    static std::string jsonQuote(const std::string& value);

    static std::string csvQuote(const std::string& value);

    static int output(const std::vector<std::pair<std::string, std::string>>& records,
                      int format, const std::string& pipeCommand);
};
