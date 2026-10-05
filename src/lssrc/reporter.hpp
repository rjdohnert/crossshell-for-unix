#pragma once

#include "lssrc.hpp"
#include "options.hpp"
#include <string>
#include <vector>

class LssrcReporter {
public:
    static std::string CsvEscape(const std::string& value);
    static std::string JsonEscape(const std::string& value);
    static void Emit(const LssrcOptions& opts, const std::vector<ServiceRow>& rows);
};
