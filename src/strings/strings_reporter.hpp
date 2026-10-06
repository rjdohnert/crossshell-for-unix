#pragma once

#include "strings.hpp"

class StringsReporter {
public:
    static int dispatch(const std::vector<std::string>& results, int format, const std::string& pipeCommand);
};
