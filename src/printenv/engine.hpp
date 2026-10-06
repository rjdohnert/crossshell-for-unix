#pragma once

#include "printenv.hpp"
#include "options.hpp"
#include "reporter.hpp"

class PrintenvEngine {
public:
    static std::wstring getEnvVar(const std::wstring& name, bool& exists);
    static int execute(const PrintenvOptions& opts);
};
