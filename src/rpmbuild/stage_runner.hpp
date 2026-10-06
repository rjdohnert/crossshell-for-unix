#pragma once

#include "rpmbuild.hpp"

class StageRunner {
public:
    static bool execute(const std::string& script, const std::string& stageName, const std::string& buildRoot);
};
