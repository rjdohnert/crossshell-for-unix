#include "mod_options.hpp"
#include "string_utils.hpp"
#include "windows_account_modifier.hpp"

bool WindowsAccountModifier::ModifyAccount(const ModOptions& opt, std::string& errorMessage) {
        std::wstring wUsername = StringUtils::ToWide(opt.username);
        PUSER_INFO_2 pUI2 = nullptr;

        NET_API_STATUS status = NetUserGetInfo(NULL, wUsername.c_str(), 2, reinterpret_cast<LPBYTE*>(&pUI2));
        if (status != NERR_Success) {
            errorMessage = FormatNetError(status);
            return false;
        }

        std::unique_ptr<USER_INFO_2, void(*)(void*)> ui2Guard(pUI2, [](void* p) {
            if (p) NetApiBufferFree(p);
        });

        std::string oldHomeDir = StringUtils::ToUtf8(pUI2->usri2_home_dir ? pUI2->usri2_home_dir : L"");

        // 1. Update Core Metadata (Comment, FullName, HomeDir, Flags)
        std::wstring wComment, wFullName, wHomeDir;

        if (opt.comment.has_value()) {
            wComment = StringUtils::ToWide(opt.comment.value());
            pUI2->usri2_comment = const_cast<LPWSTR>(wComment.c_str());
        }
        if (opt.fullName.has_value()) {
            wFullName = StringUtils::ToWide(opt.fullName.value());
            pUI2->usri2_full_name = const_cast<LPWSTR>(wFullName.c_str());
        }
        if (opt.homeDir.has_value()) {
            wHomeDir = StringUtils::ToWide(opt.homeDir.value());
            pUI2->usri2_home_dir = const_cast<LPWSTR>(wHomeDir.c_str());
        }

        // Account Lock / Unlock
        if (opt.lockAccount.has_value()) {
            if (opt.lockAccount.value()) {
                pUI2->usri2_flags |= UF_ACCOUNTDISABLE;
            } else {
                pUI2->usri2_flags &= ~UF_ACCOUNTDISABLE;
            }
        }

        // Password Expiration Policy
        if (opt.passwordNeverExpires.has_value()) {
            if (opt.passwordNeverExpires.value()) {
                pUI2->usri2_flags |= UF_DONT_EXPIRE_PASSWD;
            } else {
                pUI2->usri2_flags &= ~UF_DONT_EXPIRE_PASSWD;
            }
        }

        DWORD dwParamErr = 0;
        status = NetUserSetInfo(NULL, wUsername.c_str(), 2, reinterpret_cast<LPBYTE>(pUI2), &dwParamErr);
        if (status != NERR_Success) {
            errorMessage = "Failed setting user info: " + FormatNetError(status);
            return false;
        }

        // 2. Update Password if specified
        if (opt.password.has_value()) {
            std::wstring wPass = StringUtils::ToWide(opt.password.value());
            USER_INFO_1003 ui1003;
            ui1003.usri1003_password = const_cast<LPWSTR>(wPass.c_str());
            status = NetUserSetInfo(NULL, wUsername.c_str(), 1003, reinterpret_cast<LPBYTE>(&ui1003), NULL);
            if (status != NERR_Success) {
                errorMessage = "Failed setting password: " + FormatNetError(status);
                return false;
            }
        }

        // 3. Force Password Change at Next Logon
        if (opt.mustChangePassword.has_value() && opt.mustChangePassword.value()) {
            USER_INFO_1008 ui1008;
            ui1008.usri1008_flags = UF_SCRIPT | UF_NORMAL_ACCOUNT;
            NetUserSetInfo(NULL, wUsername.c_str(), 1008, reinterpret_cast<LPBYTE>(&ui1008), NULL);
        }

        // 4. Group Management
        if (!opt.supplementaryGroups.empty() || opt.primaryGroup.has_value()) {
            if (!UpdateGroups(opt, errorMessage)) {
                return false;
            }
        }

        // 5. Handle Home Directory Relocation (-m, --move-home)
        if (opt.moveHome && opt.homeDir.has_value() && !oldHomeDir.empty()) {
            std::string newHome = opt.homeDir.value();
            try {
                if (fs::exists(oldHomeDir) && oldHomeDir != newHome) {
                    if (opt.verbose) {
                        std::cout << "[MOVE] Relocating " << oldHomeDir << " -> " << newHome << "\n";
                    }
                    if (!fs::exists(fs::path(newHome).parent_path())) {
                        fs::create_directories(fs::path(newHome).parent_path());
                    }
                    fs::rename(oldHomeDir, newHome);
                }
            } catch (const std::exception& e) {
                errorMessage = std::string("Account updated, but home directory move failed: ") + e.what();
                return true; // Partial success
            }
        }

        // 6. Rename Account / New Login (-l, --login) - Must execute last
        if (opt.newLogin.has_value()) {
            std::wstring wNewLogin = StringUtils::ToWide(opt.newLogin.value());
            USER_INFO_0 ui0;
            ui0.usri0_name = const_cast<LPWSTR>(wNewLogin.c_str());
            status = NetUserSetInfo(NULL, wUsername.c_str(), 0, reinterpret_cast<LPBYTE>(&ui0), NULL);
            if (status != NERR_Success) {
                errorMessage = "Failed renaming account: " + FormatNetError(status);
                return false;
            }
        }

        return true;
    }

bool WindowsAccountModifier::UpdateGroups(const ModOptions& opt, std::string& errorMessage) {
        std::wstring wUsername = StringUtils::ToWide(opt.username);

        // If not appending (-a), remove the user from existing supplementary local groups
        if (!opt.appendGroups && !opt.supplementaryGroups.empty()) {
            LPLOCALGROUP_USERS_INFO_0 pGroups = nullptr;
            DWORD entriesRead = 0, totalEntries = 0;
            NET_API_STATUS status = NetUserGetLocalGroups(
                NULL, wUsername.c_str(), 0, LG_INCLUDE_INDIRECT,
                reinterpret_cast<LPBYTE*>(&pGroups), MAX_PREFERRED_LENGTH,
                &entriesRead, &totalEntries
            );

            if (status == NERR_Success && pGroups != nullptr) {
                for (DWORD i = 0; i < entriesRead; ++i) {
                    std::wstring grpName = pGroups[i].lgrui0_name;
                    std::string sGrpName = StringUtils::ToUtf8(grpName);

                    // Skip Windows default primary "Users" group during wholesale clear
                    if (_wcsicmp(grpName.c_str(), L"Users") == 0) continue;

                    LOCALGROUP_MEMBERS_INFO_3 memberInfo;
                    memberInfo.lgrmi3_domainandname = const_cast<LPWSTR>(wUsername.c_str());
                    NetLocalGroupDelMembers(NULL, grpName.c_str(), 3, reinterpret_cast<LPBYTE>(&memberInfo), 1);
                }
                NetApiBufferFree(pGroups);
            }
        }

        // Add to specified supplementary groups
        std::vector<std::string> groupsToAdd = opt.supplementaryGroups;
        if (opt.primaryGroup.has_value()) {
            groupsToAdd.push_back(opt.primaryGroup.value());
        }

        for (const auto& grp : groupsToAdd) {
            std::wstring wGrp = StringUtils::ToWide(grp);
            LOCALGROUP_MEMBERS_INFO_3 memberInfo;
            memberInfo.lgrmi3_domainandname = const_cast<LPWSTR>(wUsername.c_str());

            NET_API_STATUS s = NetLocalGroupAddMembers(NULL, wGrp.c_str(), 3, reinterpret_cast<LPBYTE>(&memberInfo), 1);
            if (s != NERR_Success && s != ERROR_MEMBER_IN_ALIAS) {
                if (opt.verbose) {
                    std::cerr << "[WARNING] Could not add user to group '" << grp << "': " << FormatNetError(s) << "\n";
                }
            }
        }

        return true;
    }

std::string WindowsAccountModifier::FormatNetError(NET_API_STATUS status) {
        switch (status) {
        case NERR_UserNotFound: return "User account does not exist.";
        case NERR_GroupNotFound: return "Specified local group does not exist.";
        case NERR_PasswordTooShort: return "Password does not meet length/complexity policies.";
        case ERROR_ACCESS_DENIED: return "Access Denied. Run this tool as an Administrator.";
        case ERROR_INVALID_PARAMETER: return "Invalid parameter specified.";
        default: return "NetAPI error code: " + std::to_string(status);
        }
    }
