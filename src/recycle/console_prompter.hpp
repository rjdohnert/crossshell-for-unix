#pragma once

#include "recycle.hpp"

class ConsolePrompter {
public:
    static bool PromptUser(const fs::path& path);
};
