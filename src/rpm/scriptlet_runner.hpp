#pragma once

#include "rpm.hpp"

class ScriptletRunner {
public:
    static bool execute(const std::string& script, const std::string& phase);
};
