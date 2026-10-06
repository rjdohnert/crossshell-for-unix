#pragma once

#include "user_account.hpp"
#include "useradd.hpp"

class WindowsUserManager {
public:
    static bool CreateAccount(const UserAccount& user, std::string& errorMessage);

private:
    static bool AddUserToGroup(const std::string& username, const std::string& groupName);

    static std::string FormatNetError(NET_API_STATUS status);
};
