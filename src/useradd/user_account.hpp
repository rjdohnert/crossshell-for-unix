#pragma once

#include "useradd.hpp"

struct UserAccount {
    std::string username;
    std::string password;
    std::string comment;
    std::string fullName;
    std::string homeDir;
    std::string primaryGroup = "Users";
    std::vector<std::string> supplementaryGroups;
    bool createHomeDir = true;
    bool passwordNeverExpires = false;
    bool accountDisabled = false;
    bool mustChangePassword = false;
    bool isSystemAccount = false;
};
