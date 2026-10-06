#pragma once

#include "delete_options.hpp"
#include "userdel.hpp"

class WindowsUserManager {
public:
    static bool TerminateUserProcesses(const std::string& username, bool verbose);

    static bool DeleteUserProfile(const std::string& username, bool verbose);

    static bool DeleteHomeDirectory(const std::string& username, const std::string& rootDir, bool verbose);

    static bool DeleteAccount(const DeleteOptions& opt, std::string& errorMessage);

private:
    static std::string FormatNetError(NET_API_STATUS status);
};
