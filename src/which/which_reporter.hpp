#pragma once

#include "which.hpp"

class WhichReporter {
public:
    static int dispatch(const std::vector<std::pair<std::string, std::string>>& matches,
                        int format, const std::string& pipeCommand);
};
