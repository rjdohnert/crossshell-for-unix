#pragma once

#include "usermod.hpp"

struct ModOptions {
    std::string username;
    std::optional<std::string> newLogin;
    std::optional<std::string> comment;
    std::optional<std::string> fullName;
    std::optional<std::string> homeDir;
    std::optional<std::string> password;
    std::optional<std::string> primaryGroup;
    std::vector<std::string> supplementaryGroups;
    bool appendGroups = false;
    bool moveHome = false;
    std::optional<bool> lockAccount;            // true = lock, false = unlock
    std::optional<bool> passwordNeverExpires;
    std::optional<bool> mustChangePassword;
    bool verbose = false;
    bool pipeMode = false;
    bool stdinPassword = false;
};
