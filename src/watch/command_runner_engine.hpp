#pragma once

#include "watch.hpp"

class CommandRunnerEngine {
public:
    static int RunOnce(const std::wstring& childCmdLine);
};
