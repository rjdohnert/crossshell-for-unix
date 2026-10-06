#pragma once

#include "unzip.hpp"

bool simple_match(const std::string& pattern, const std::string& str);

bool should_extract(const std::string& filename, const std::vector<std::string>& filters);
