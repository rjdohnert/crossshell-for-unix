#pragma once

#include "program_config.hpp"
#include "supervisord.hpp"

struct ParseConfigResult {
    std::vector<ProgramConfig> configs;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};
