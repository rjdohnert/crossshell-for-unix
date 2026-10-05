/*
BSD 3-Clause License
/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <lm.h>
#include <sddl.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <optional>
#include <filesystem>
#include <algorithm>
#include <map>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

// ============================================================================
// Utility / String Helpers
// ============================================================================
class StringUtils {
public:
    static std::wstring ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstr[0], size_needed);
        return wstr;
    }

    static std::string ToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
        std::string str(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &str[0], size_needed, NULL, NULL);
        return str;
    }

    static std::string Trim(std::string str) {
        str.erase(0, str.find_first_not_of(" \t\r\n"));
        str.erase(str.find_last_not_of(" \t\r\n") + 1);
        return str;
    }

    static std::vector<std::string> Split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            token = Trim(token);
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }
};

// ============================================================================
// Model: ModOptions
// ============================================================================
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

// ============================================================================
// System Facade: WindowsAccountModifier (NetAPI32 Wrapper)
// ============================================================================
class WindowsAccountModifier {
public:
    static bool ModifyAccount(const ModOptions& opt, std::string& errorMessage) {
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

private:
    static bool UpdateGroups(const ModOptions& opt, std::string& errorMessage) {
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

    static std::string FormatNetError(NET_API_STATUS status) {
        switch (status) {
        case NERR_UserNotFound: return "User account does not exist.";
        case NERR_GroupNotFound: return "Specified local group does not exist.";
        case NERR_PasswordTooShort: return "Password does not meet length/complexity policies.";
        case ERROR_ACCESS_DENIED: return "Access Denied. Run this tool as an Administrator.";
        case ERROR_INVALID_PARAMETER: return "Invalid parameter specified.";
        default: return "NetAPI error code: " + std::to_string(status);
        }
    }
};

// ============================================================================
// CLI Option Parser & Help Generator
// ============================================================================
class CommandLineParser {
public:
    static void PrintHelp() {
           std::cout << R"(usermod(1)              CrossShell for UNIX Reference Manual                   usermod(1)

    NAME
        usermod - modify a local Windows user account

    SYNOPSIS
        usermod [OPTIONS] LOGIN
        usermod --pipe [OPTIONS]
        COMMAND | usermod --pipe

    DESCRIPTION
        Modifies the configuration of a local Windows user account. Changes may be
        supplied as command-line options or as key-value records through standard
        input in --pipe mode.

    OPTIONS
        -a, --append
            Append the user to supplementary groups specified with -G.

        -c, --comment COMMENT
            Set the user's comment or GECOS field.

        --full-name NAME
            Set the user's full display name.

        -d, --home-dir HOME_DIR
            Set the user's home directory.

        -m, --move-home
            Move the existing home directory contents to the new -d location.

        -g, --gid, --group GROUP
            Set the primary local group.

        -G, --groups GROUPS
            Set supplementary groups as a comma-separated list.

        -l, --login NEW_LOGIN
            Change the user's login name.

        -L, --lock
            Lock the password and disable the account.

        -U, --unlock
            Unlock the password and re-enable the account.

        -p, --password PASSWORD
            Assign a new cleartext password.

        -f, --force-password-change
            Require a password change at next logon.

        --never-expires
            Set the password to never expire.

        --expires
            Re-enable the password expiration schedule.

        --pipe
            Read records as <username>:<key>=<value>,<key>=<value>. Supported keys
            include comment, fullname, homedir, pass, groups, and lock.

        --stdin-password
            Read a single user's new password from standard input.

        -v, --verbose
            Enable detailed step-by-step diagnostic output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        --version
            Display version and licensing information.

    EXIT STATUS
        0          Success.
        1          General failure or user account not found.
        2          Invalid command syntax or usage.
        12         Access denied; an elevated administrative shell is required.

    EXAMPLES
        usermod -l jsmith --full-name "John Smith" jdoe
            Rename an account and assign a new display name.

        usermod -L bad_actor
            Lock and disable a user account.

        usermod -d "D:\Profiles\jsmith" -m jsmith
            Move the existing home directory to a new location.

        usermod -a -G "Administrators,Remote Desktop Users" jsmith
            Append a user to two supplementary groups.

        powershell -Command "Read-Host -AsSecureString" | usermod --stdin-password jsmith
            Supply a password dynamically through standard input.

        echo jsmith:lock=true,comment=Suspended | usermod --pipe --verbose
            Apply batch modifications from a configuration stream.

    CrossShell for UNIX                                                     usermod(1)
    )";
    }

    static void PrintVersion() {
        std::cout << "usermod 2.0.0\n";
        std::cout << "Copyright (C) 2026, Roberto J Dohnert.\n";
    }
};

// ============================================================================
// Pipeline Controller
// ============================================================================
class InputPipeline {
public:
    static bool IsPipeActive() {
        return !_isatty(_fileno(stdin));
    }

    static int ProcessBatchPipe(const ModOptions& baseOpt) {
        std::string line;
        size_t total = 0;
        size_t successful = 0;

        // Input format: username:key=value,key=value
        while (std::getline(std::cin, line)) {
            line = StringUtils::Trim(line);
            if (line.empty() || line[0] == '#') continue;

            auto colonIdx = line.find(':');
            if (colonIdx == std::string::npos) continue;

            std::string user = StringUtils::Trim(line.substr(0, colonIdx));
            std::string payload = line.substr(colonIdx + 1);

            ModOptions opt = baseOpt;
            opt.username = user;

            auto pairs = StringUtils::Split(payload, ',');
            for (const auto& pair : pairs) {
                auto eqIdx = pair.find('=');
                if (eqIdx == std::string::npos) continue;
                std::string k = StringUtils::Trim(pair.substr(0, eqIdx));
                std::string v = StringUtils::Trim(pair.substr(eqIdx + 1));

                if (k == "comment") opt.comment = v;
                else if (k == "fullname") opt.fullName = v;
                else if (k == "homedir") opt.homeDir = v;
                else if (k == "pass") opt.password = v;
                else if (k == "groups") opt.supplementaryGroups = StringUtils::Split(v, ';');
                else if (k == "lock") opt.lockAccount = (v == "true" || v == "1");
                else if (k == "login") opt.newLogin = v;
            }

            total++;
            std::string err;
            if (WindowsAccountModifier::ModifyAccount(opt, err)) {
                successful++;
                if (baseOpt.verbose) {
                    std::cout << "[SUCCESS] Modified account: " << user << "\n";
                }
            } else {
                std::cerr << "[ERROR] Failed updating user '" << user << "': " << err << "\n";
            }
        }

        std::cout << "Batch modification finished: " << successful << "/" << total << " accounts updated.\n";
        return (successful == total) ? 0 : 1;
    }
};

// ============================================================================
// Application Orchestrator
// ============================================================================
class Application {
public:
    int Run(int argc, char* argv[]) {
        if (argc < 2) {
            if (InputPipeline::IsPipeActive()) {
                ModOptions defaultOpt;
                return InputPipeline::ProcessBatchPipe(defaultOpt);
            }
            CommandLineParser::PrintHelp();
            return 2;
        }

        ModOptions opt;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                CommandLineParser::PrintHelp();
                return 0;
            } else if (arg == "--version") {
                CommandLineParser::PrintVersion();
                return 0;
            } else if (arg == "-v" || arg == "--verbose") {
                opt.verbose = true;
            } else if (arg == "--pipe") {
                opt.pipeMode = true;
            } else if (arg == "--stdin-password") {
                opt.stdinPassword = true;
            } else if (arg == "-a" || arg == "--append") {
                opt.appendGroups = true;
            } else if (arg == "-m" || arg == "--move-home") {
                opt.moveHome = true;
            } else if (arg == "-L" || arg == "--lock") {
                opt.lockAccount = true;
            } else if (arg == "-U" || arg == "--unlock") {
                opt.lockAccount = false;
            } else if (arg == "-f" || arg == "--force-password-change") {
                opt.mustChangePassword = true;
            } else if (arg == "--never-expires") {
                opt.passwordNeverExpires = true;
            } else if (arg == "--expires") {
                opt.passwordNeverExpires = false;
            } else if (arg == "-c" || arg == "--comment") {
                if (++i < argc) opt.comment = argv[i];
            } else if (arg == "--full-name") {
                if (++i < argc) opt.fullName = argv[i];
            } else if (arg == "-d" || arg == "--home-dir") {
                if (++i < argc) opt.homeDir = argv[i];
            } else if (arg == "-g" || arg == "--gid" || arg == "--group") {
                if (++i < argc) opt.primaryGroup = argv[i];
            } else if (arg == "-G" || arg == "--groups") {
                if (++i < argc) opt.supplementaryGroups = StringUtils::Split(argv[i], ',');
            } else if (arg == "-l" || arg == "--login") {
                if (++i < argc) opt.newLogin = argv[i];
            } else if (arg == "-p" || arg == "--password") {
                if (++i < argc) opt.password = argv[i];
            } else if (!arg.empty() && arg[0] != '-') {
                opt.username = arg;
            } else {
                std::cerr << "usermod: unrecognized option '" << arg << "'\n";
                std::cerr << "Try 'usermod --help' for more information.\n";
                return 2;
            }
        }

        if (opt.pipeMode) {
            return InputPipeline::ProcessBatchPipe(opt);
        }

        if (opt.username.empty()) {
            std::cerr << "usermod: error: no username specified. Use --help for usage.\n";
            return 2;
        }

        // Handle reading password from standard input pipe
        if (opt.stdinPassword) {
            std::string pass;
            if (std::getline(std::cin, pass)) {
                opt.password = StringUtils::Trim(pass);
            }
        }

        if (opt.verbose) {
            std::cout << "Target User: " << opt.username << "\n";
            if (opt.newLogin.has_value()) std::cout << "  New Login: " << opt.newLogin.value() << "\n";
            if (opt.homeDir.has_value()) std::cout << "  New Home Dir: " << opt.homeDir.value() << "\n";
            if (opt.lockAccount.has_value()) std::cout << "  Account Lock: " << (opt.lockAccount.value() ? "Enabled" : "Disabled") << "\n";
        }

        std::string err;
        if (!WindowsAccountModifier::ModifyAccount(opt, err)) {
            std::cerr << "usermod: " << err << "\n";
            return 1;
        }

        if (opt.verbose) {
            std::cout << "usermod: Account '" << opt.username << "' modified successfully.\n";
        }

        return 0;
    }
};

// ============================================================================
// Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    Application app;
    return app.Run(argc, argv);
}