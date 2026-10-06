#pragma once

#include "wipe.hpp"

class UserPrompt {
public:
    static bool ConfirmAction(const std::string& target);
};
