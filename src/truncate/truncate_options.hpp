#pragma once

#include "size_calculator.hpp"
#include "truncate.hpp"

struct CliOptions {
    bool no_create = false;
    bool has_size = false;
    SizeCalculator size_calc;
    std::wstring ref_file;
    std::vector<std::string> files;
    bool show_help = false;
    bool show_version = false;
};
