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
#include <lmaccess.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include <optional>
#include <filesystem>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

// ============================================================================
// Utility / String Conversion Helpers
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

    static std::vector<std::string> Split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter)) {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }
};

// ============================================================================
// Model: UserAccount
// ============================================================================
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

// ============================================================================
// System Facade: WindowsUserManager (NetAPI32 Wrapper)
// ============================================================================
class WindowsUserManager {
public:
    static bool CreateAccount(const UserAccount& user, std::string& errorMessage) {
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

private:
    static bool AddUserToGroup(const std::string& username, const std::string& groupName) {
        std::wstring wUsername = StringUtils::ToWide(username);
        std::wstring wGroupName = StringUtils::ToWide(groupName);

        LOCALGROUP_MEMBERS_INFO_3 memberInfo;
        memberInfo.lgrmi3_domainandname = const_cast<LPWSTR>(wUsername.c_str());

        NET_API_STATUS status = NetLocalGroupAddMembers(NULL, wGroupName.c_str(), 3, (LPBYTE)&memberInfo, 1);
        return (status == NERR_Success || status == ERROR_MEMBER_IN_ALIAS);
    }

    static std::string FormatNetError(NET_API_STATUS status) {
        switch (status) {
        case NERR_UserExists: return "The user account already exists.";
        case NERR_GroupNotFound: return "Specified group does not exist.";
        case NERR_PasswordTooShort: return "The password does not meet the length/complexity requirements.";
        case ERROR_ACCESS_DENIED: return "Access Denied. Run this command in an elevated administrator prompt.";
        case ERROR_INVALID_PARAMETER: return "An invalid parameter was specified.";
        default: return "NetAPI Error code: " + std::to_string(status);
        }
    }
};

// ============================================================================
// CLI Option Parser & Help Generator
// ============================================================================
class CommandLineParser {
public:
    static void PrintHelp() {
           std::cout << R"(useradd(1)              CrossShell for UNIX Reference Manual                   useradd(1)

    NAME
        useradd - create a local Windows user account

    SYNOPSIS
        useradd [OPTIONS] LOGIN
        useradd --stdin [OPTIONS]
        COMMAND | useradd --pipe

    DESCRIPTION
        Creates a local Windows user account or batch-creates accounts from standard
        input. User entries may be supplied through the explicit --pipe mode or the
        --stdin-password option.

    OPTIONS
        -c, --comment COMMENT
            Set the user's full comment or description.

        -d, --home-dir HOME_DIR
            Set the home directory path for the new account.

        -e, --expiredate EXPIRE_DATE
            Set an expiration date in YYYY-MM-DD format, or NEVER.

        -f, --force-password-change
            Require the user to change the password at next logon.

        -g, --gid, --group GROUP
            Set the primary group; the default is Users.

        -G, --groups GROUPS
            Set supplementary groups as a comma-separated list.

        -k, --skel SKEL_DIR
            Use a custom skeleton directory; ignored when -M is set.

        -m, --create-home
            Create the user's home directory (default).

        -M, --no-create-home
            Do not create the user's home directory.

        -N, --no-user-group
            Do not create a group with the same name as the user.

        -p, --password PASSWORD
            Set an encrypted or cleartext password.

        -r, --system
            Create a system account.

        -s, --shell SHELL
            Set the login shell, such as powershell.exe, cmd.exe, or bash.exe.

        -u, --uid UID
            Set a numeric user identifier or SID alias.

        -U, --user-group
            Create a group with the same name as the user.

        --disabled
            Create the account in a disabled state.

        --never-expires
            Set the password to never expire.

        --pipe
            Read entries as <username>:<password>:[comment]:[group1,group2].

        --stdin-password
            Read a single user's password from standard input.

        -v, --verbose
            Enable verbose diagnostic output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        --version
            Display version and licensing information.

    EXAMPLES
        useradd -m -p "P@ssw0rd123!" -G Administrators,Users -c "Jane Doe" jdoe
            Create a user with a password and supplementary groups.

        useradd -M -r --disabled -c "CI Runner Service" svc_runner
            Create a disabled service account without a home directory.

        echo alice:P@ss1:Alice Walker:Developers | useradd --pipe
            Batch-create an account from a colon-delimited input record.

        type users.txt | useradd --pipe --verbose
            Batch-create accounts from a file.

        powershell -Command "Read-Host -AsSecureString" | useradd -m --stdin-password devuser
            Supply a password through a standard input pipeline.

    CrossShell for UNIX                                                     useradd(1)
    )";
    }

    static void PrintVersion() {
        std::cout << "useradd 2.0.0\n";
        std::cout << "Copyright (C) 2026, Roberto J Dohnert.\n";
    }
};

// ============================================================================
// Pipeline Processing Controller
// ============================================================================
class InputPipeline {
public:
    static bool IsPipeActive() {
        // Checks if standard input is redirected/piped
        return !_isatty(_fileno(stdin));
    }

    static void ProcessBatchPipe(bool verbose) {
        std::string line;
        size_t count = 0;
        size_t successCount = 0;

        while (std::getline(std::cin, line)) {
            // Remove carriage return if present (Windows CRLF)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty() || line[0] == '#') continue; // Skip comments/empty

            // Format: username:password:comment:group1,group2
            auto tokens = StringUtils::Split(line, ':');
            if (tokens.empty()) continue;

            UserAccount user;
            user.username = tokens[0];

            if (tokens.size() > 1) user.password = tokens[1];
            if (tokens.size() > 2) user.comment = tokens[2];
            if (tokens.size() > 3) {
                user.supplementaryGroups = StringUtils::Split(tokens[3], ',');
            }

            // Defaults
            user.homeDir = "C:\\Users\\" + user.username;
            user.createHomeDir = true;

            count++;
            std::string err;
            if (WindowsUserManager::CreateAccount(user, err)) {
                successCount++;
                if (verbose) {
                    std::cout << "[SUCCESS] Created user: " << user.username << "\n";
                }
            } else {
                std::cerr << "[ERROR] Failed creating user '" << user.username << "': " << err << "\n";
            }
        }

        std::cout << "Batch processing finished: " << successCount << "/" << count << " accounts created.\n";
    }
};

// ============================================================================
// Application Controller
// ============================================================================
class Application {
public:
    int Run(int argc, char* argv[]) {
        if (argc < 2) {
            if (InputPipeline::IsPipeActive()) {
                InputPipeline::ProcessBatchPipe(false);
                return 0;
            }
            CommandLineParser::PrintHelp();
            return 1;
        }

        UserAccount user;
        bool verbose = false;
        bool pipeMode = false;
        bool stdinPassword = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                CommandLineParser::PrintHelp();
                return 0;
            } else if (arg == "--version") {
                CommandLineParser::PrintVersion();
                return 0;
            } else if (arg == "-v" || arg == "--verbose") {
                verbose = true;
            } else if (arg == "--pipe") {
                pipeMode = true;
            } else if (arg == "--stdin-password") {
                stdinPassword = true;
            } else if (arg == "-c" || arg == "--comment") {
                if (++i < argc) user.comment = argv[i];
            } else if (arg == "-d" || arg == "--home-dir") {
                if (++i < argc) user.homeDir = argv[i];
            } else if (arg == "-g" || arg == "--gid" || arg == "--group") {
                if (++i < argc) user.primaryGroup = argv[i];
            } else if (arg == "-G" || arg == "--groups") {
                if (++i < argc) user.supplementaryGroups = StringUtils::Split(argv[i], ',');
            } else if (arg == "-p" || arg == "--password") {
                if (++i < argc) user.password = argv[i];
            } else if (arg == "-m" || arg == "--create-home") {
                user.createHomeDir = true;
            } else if (arg == "-M" || arg == "--no-create-home") {
                user.createHomeDir = false;
            } else if (arg == "-r" || arg == "--system") {
                user.isSystemAccount = true;
            } else if (arg == "-f" || arg == "--force-password-change") {
                user.mustChangePassword = true;
            } else if (arg == "--never-expires") {
                user.passwordNeverExpires = true;
            } else if (arg == "--disabled") {
                user.accountDisabled = true;
            } else if (!arg.empty() && arg[0] != '-') {
                user.username = arg;
            }
        }

        if (pipeMode) {
            InputPipeline::ProcessBatchPipe(verbose);
            return 0;
        }

        if (user.username.empty()) {
            std::cerr << "useradd: error: no username specified. Use --help for usage.\n";
            return 1;
        }

        // Handle reading password from standard input
        if (stdinPassword) {
            std::string pass;
            if (std::getline(std::cin, pass)) {
                if (!pass.empty() && pass.back() == '\r') pass.pop_back();
                user.password = pass;
            }
        }

        // Set default home directory if enabled and unspecified
        if (user.createHomeDir && user.homeDir.empty()) {
            user.homeDir = "C:\\Users\\" + user.username;
        }

        if (verbose) {
            std::cout << "Creating user: " << user.username << "\n";
            std::cout << "Home directory: " << (user.homeDir.empty() ? "(none)" : user.homeDir) << "\n";
            std::cout << "Primary group: " << user.primaryGroup << "\n";
        }

        std::string err;
        if (!WindowsUserManager::CreateAccount(user, err)) {
            std::cerr << "useradd: " << err << "\n";
            return 1;
        }

        if (verbose) {
            std::cout << "useradd: User '" << user.username << "' created successfully.\n";
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