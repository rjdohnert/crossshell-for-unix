#include "string_utils.hpp"
#include "user_account.hpp"
#include "windows_user_manager.hpp"

bool WindowsUserManager::CreateAccount(const UserAccount& user, std::string& errorMessage) {
        std::wstring wUsername = StringUtils::ToWide(user.username);
        std::wstring wPassword = StringUtils::ToWide(user.password);
        std::wstring wComment = StringUtils::ToWide(user.comment);
        std::wstring wFullName = StringUtils::ToWide(user.fullName);
        std::wstring wHomeDir = StringUtils::ToWide(user.homeDir);

        USER_INFO_2 ui;
        ZeroMemory(&ui, sizeof(USER_INFO_2));

        ui.usri2_name = const_cast<LPWSTR>(wUsername.c_str());
        ui.usri2_password = const_cast<LPWSTR>(wPassword.c_str());
        ui.usri2_priv = USER_PRIV_USER;
        ui.usri2_home_dir = const_cast<LPWSTR>(wHomeDir.c_str());
        ui.usri2_comment = const_cast<LPWSTR>(wComment.c_str());
        ui.usri2_full_name = const_cast<LPWSTR>(wFullName.c_str());
        ui.usri2_flags = UF_SCRIPT | UF_NORMAL_ACCOUNT;
        ui.usri2_acct_expires = TIMEQ_FOREVER;
        ui.usri2_max_storage = USER_MAXSTORAGE_UNLIMITED;

        if (user.passwordNeverExpires) {
            // The NetAPI32 constant is named UF_DONT_EXPIRE_PASSWD.
            ui.usri2_flags |= UF_DONT_EXPIRE_PASSWD;
        }
        if (user.accountDisabled) {
            ui.usri2_flags |= UF_ACCOUNTDISABLE;
        }

        DWORD dwLevel = 2;
        DWORD dwError = 0;
        NET_API_STATUS nStatus = NetUserAdd(NULL, dwLevel, (LPBYTE)&ui, &dwError);

        if (nStatus != NERR_Success) {
            errorMessage = FormatNetError(nStatus);
            return false;
        }

        // Post-creation flags (Force password change)
        if (user.mustChangePassword) {
            USER_INFO_1008 ui1008;
            ui1008.usri1008_flags = UF_SCRIPT | UF_NORMAL_ACCOUNT;
            NetUserSetInfo(NULL, wUsername.c_str(), 1008, (LPBYTE)&ui1008, NULL);
        }

        // Assign to Primary & Supplementary Local Groups
        std::vector<std::string> allGroups = user.supplementaryGroups;
        if (!user.primaryGroup.empty()) {
            allGroups.push_back(user.primaryGroup);
        }

        for (const auto& group : allGroups) {
            if (!group.empty()) {
                AddUserToGroup(user.username, group);
            }
        }

        // Create Home Directory if specified
        if (user.createHomeDir && !user.homeDir.empty()) {
            try {
                if (!fs::exists(user.homeDir)) {
                    fs::create_directories(user.homeDir);
                }
            } catch (const std::exception& e) {
                errorMessage = std::string("Account created, but home directory creation failed: ") + e.what();
                return true; // Partially successful
            }
        }

        return true;
    }

bool WindowsUserManager::AddUserToGroup(const std::string& username, const std::string& groupName) {
        std::wstring wUsername = StringUtils::ToWide(username);
        std::wstring wGroupName = StringUtils::ToWide(groupName);

        LOCALGROUP_MEMBERS_INFO_3 memberInfo;
        memberInfo.lgrmi3_domainandname = const_cast<LPWSTR>(wUsername.c_str());

        NET_API_STATUS status = NetLocalGroupAddMembers(NULL, wGroupName.c_str(), 3, (LPBYTE)&memberInfo, 1);
        return (status == NERR_Success || status == ERROR_MEMBER_IN_ALIAS);
    }

std::string WindowsUserManager::FormatNetError(NET_API_STATUS status) {
        switch (status) {
        case NERR_UserExists: return "The user account already exists.";
        case NERR_GroupNotFound: return "Specified group does not exist.";
        case NERR_PasswordTooShort: return "The password does not meet the length/complexity requirements.";
        case ERROR_ACCESS_DENIED: return "Access Denied. Run this command in an elevated administrator prompt.";
        case ERROR_INVALID_PARAMETER: return "An invalid parameter was specified.";
        default: return "NetAPI Error code: " + std::to_string(status);
        }
    }
