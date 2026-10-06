#pragma once

#include "test.hpp"

struct TestOptions {
    bool isBracket = false;
    int format = 0;
    std::wstring pipeCommand;
    std::vector<std::wstring> tokens;
};
