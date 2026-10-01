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
#include <userenv.h>
#include <sddl.h>
#include <tlhelp32.h>
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

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "userenv.lib")

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
// Model: DeleteOptions
// ============================================================================
struct DeleteOptions {
    std::string username;
    bool removeHomeAndProfile = false;
    bool force = false;
    bool verbose = false;
    bool pipeMode = false;
    std::string rootDir = "";
};

// ============================================================================
// Windows Security & SID Subsystem
// ============================================================================
class SecurityUtils {
public:
    static std::optional<std::wstring> GetUserSidString(const std::string& username) {
        std::wstring wUsername = StringUtils::ToWide(username);
        DWORD sidSize = 0;
        DWORD domainSize = 0;
        SID_NAME_USE sidType;

        LookupAccountNameW(NULL, wUsername.c_str(), NULL, &sidSize, NULL, &domainSize, &sidType);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return std::nullopt;
        }

        std::vector<BYTE> sidBuffer(sidSize);
        std::vector<WCHAR> domainBuffer(domainSize);
        PSID pSid = reinterpret_cast<PSID>(sidBuffer.data());

        if (!LookupAccountNameW(NULL, wUsername.c_str(), pSid, &sidSize, domainBuffer.data(), &domainSize, &sidType)) {
            return std::nullopt;
        }

        LPWSTR stringSid = NULL;
        if (ConvertSidToStringSidW(pSid, &stringSid)) {
            std::wstring result(stringSid);
            LocalFree(stringSid);
            return result;
        }

        return std::nullopt;
    }
};

// ============================================================================
// System Facade: WindowsUserManager (Deletion & Cleanup)
// ============================================================================
class WindowsUserManager {
public:
    static bool TerminateUserProcesses(const std::string& username, bool verbose) {
        auto optSid = SecurityUtils::GetUserSidString(username);
        if (!optSid.has_value()) return false;

        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (pe.th32ProcessID <= 4) continue; // Skip System/Idle

                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    HANDLE hToken = NULL;
                    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
                        DWORD len = 0;
                        GetTokenInformation(hToken, TokenUser, NULL, 0, &len);
                        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                            std::vector<BYTE> buffer(len);
                            PTOKEN_USER pTokenUser = reinterpret_cast<PTOKEN_USER>(buffer.data());
                            if (GetTokenInformation(hToken, TokenUser, pTokenUser, len, &len)) {
                                LPWSTR procSidStr = NULL;
                                if (ConvertSidToStringSidW(pTokenUser->User.Sid, &procSidStr)) {
                                    if (optSid.value() == procSidStr) {
                                        if (verbose) {
                                            std::wcout << L"[FORCE] Terminating process: " << pe.szExeFile 
                                                       << L" (PID: " << pe.th32ProcessID << L")\n";
                                        }
                                        TerminateProcess(hProcess, 1);
                                    }
                                    LocalFree(procSidStr);
                                }
                            }
                        }
                        CloseHandle(hToken);
                    }
                    CloseHandle(hProcess);
                }
            } while (Process32NextW(hSnapshot, &pe));
        }

        CloseHandle(hSnapshot);
        return true;
    }

    static bool DeleteUserProfile(const std::string& username, bool verbose) {
        auto optSid = SecurityUtils::GetUserSidString(username);
        if (!optSid.has_value()) return false;

        if (verbose) {
            std::wcout << L"Removing User Profile with SID: " << optSid.value() << L"\n";
        }

        // Delete official user profile via Win32 UserEnv API
        BOOL status = DeleteProfileW(optSid.value().c_str(), NULL, NULL);
        return (status == TRUE);
    }

    static bool DeleteHomeDirectory(const std::string& username, const std::string& rootDir, bool verbose) {
        fs::path userHome = rootDir.empty() 
            ? (fs::path("C:\\Users") / username) 
            : (fs::path(rootDir) / username);

        try {
            if (fs::exists(userHome)) {
                if (verbose) {
                    std::cout << "Removing home directory: " << userHome.string() << "\n";
                }
                fs::remove_all(userHome);
                return true;
            }
        } catch (const std::exception& e) {
            if (verbose) {
                std::cerr << "[WARNING] Failed removing home directory: " << e.what() << "\n";
            }
            return false;
        }
        return false;
    }

    static bool DeleteAccount(const DeleteOptions& opt, std::string& errorMessage) {
        std::wstring wUsername = StringUtils::ToWide(opt.username);

        // Terminate active processes if force requested
        if (opt.force) {
            TerminateUserProcesses(opt.username, opt.verbose);
        }

        // Retrieve and delete Windows profile and folder if -r was supplied
        if (opt.removeHomeAndProfile) {
            DeleteUserProfile(opt.username, opt.verbose);
            DeleteHomeDirectory(opt.username, opt.rootDir, opt.verbose);
        }

        // Remove the user account from the Windows SAM database
        NET_API_STATUS nStatus = NetUserDel(NULL, wUsername.c_str());

        if (nStatus != NERR_Success) {
            errorMessage = FormatNetError(nStatus);
            return false;
        }

        return true;
    }

private:
    static std::string FormatNetError(NET_API_STATUS status) {
        switch (status) {
        case NERR_UserNotFound: return "User account does not exist.";
        case ERROR_ACCESS_DENIED: return "Access Denied. Run this command in an elevated administrator terminal.";
        case ERROR_INVALID_PARAMETER: return "Invalid parameter specified.";
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
           std::cout << R"(userdel(1)              CrossShell for UNIX Reference Manual                   userdel(1)

    NAME
        userdel - delete a local Windows user account

    SYNOPSIS
        userdel [OPTIONS] LOGIN
        userdel --pipe [OPTIONS]
        COMMAND | userdel [OPTIONS]

    DESCRIPTION
        Deletes a local Windows user account and optionally removes its home
        directory and profile store. Piped input is detected automatically or can
        be selected explicitly with --pipe.

    OPTIONS
        -f, --force
            Force removal even when the user is logged in; terminate processes
            owned by the user.

        -r, --remove
            Remove the home directory and user profile store.

        -R, --root CHROOT_DIR
            Set the directory prefix used for home path resolution.

        -Z, --selinux-user
            POSIX compatibility option; ignored on Windows.

        --pipe
            Read account names to delete from standard input.

        -v, --verbose
            Enable detailed diagnostic and step-by-step output.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        --version
            Display version and licensing information.

    PIPING AND STREAMING
        Input may contain plain usernames, comma-separated values, or colon-delimited
        lists. Blank lines and comment records are ignored.

    EXIT STATUS
        0          Success.
        1          General failure or account not found.
        2          Invalid command syntax or usage.
        12         Access denied; administrative privileges are required.

    EXAMPLES
        userdel testuser
            Delete a local user without touching profile files.

        userdel -r testuser
            Delete a user and remove C:\Users\testuser.

        userdel -f -r loggedin_user
            Terminate the user's processes, delete the account, and remove its profile.

        type deprovision_list.txt | userdel -r --verbose
            Batch-delete users from a file.

        echo user1,user2,user3 | userdel -r --pipe
            Batch-delete comma-separated users from a pipeline.

    CrossShell for UNIX                                                     userdel(1)
    )";
    }

    static void PrintVersion() {
        std::cout << "userdel 2.0.0\n";
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

    static int ProcessBatchPipe(const DeleteOptions& baseOpt) {
        std::string line;
        size_t count = 0;
        size_t successCount = 0;

        while (std::getline(std::cin, line)) {
            line = StringUtils::Trim(line);
            if (line.empty() || line[0] == '#') continue;

            // Handle comma or whitespace separated lines
            std::vector<std::string> userList;
            if (line.find(',') != std::string::npos) {
                userList = StringUtils::Split(line, ',');
            } else if (line.find(':') != std::string::npos) {
                userList = StringUtils::Split(line, ':');
            } else {
                userList.push_back(line);
            }

            for (const auto& rawUser : userList) {
                std::string targetUser = StringUtils::Trim(rawUser);
                if (targetUser.empty()) continue;

                count++;
                DeleteOptions opt = baseOpt;
                opt.username = targetUser;

                std::string err;
                if (WindowsUserManager::DeleteAccount(opt, err)) {
                    successCount++;
                    if (baseOpt.verbose) {
                        std::cout << "[SUCCESS] Deleted user: " << targetUser << "\n";
                    }
                } else {
                    std::cerr << "[ERROR] Failed deleting user '" << targetUser << "': " << err << "\n";
                }
            }
        }

        std::cout << "Batch deletion finished: " << successCount << "/" << count << " accounts deleted.\n";
        return (successCount == count) ? 0 : 1;
    }
};

// ============================================================================
// Application Orchestrator
// ============================================================================
class Application {
public:
    int Run(int argc, char* argv[]) {
        DeleteOptions options;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                CommandLineParser::PrintHelp();
                return 0;
            } else if (arg == "--version") {
                CommandLineParser::PrintVersion();
                return 0;
            } else if (arg == "-v" || arg == "--verbose") {
                options.verbose = true;
            } else if (arg == "-f" || arg == "--force") {
                options.force = true;
            } else if (arg == "-r" || arg == "--remove") {
                options.removeHomeAndProfile = true;
            } else if (arg == "--pipe") {
                options.pipeMode = true;
            } else if (arg == "-R" || arg == "--root") {
                if (++i < argc) options.rootDir = argv[i];
            } else if (arg == "-Z" || arg == "--selinux-user") {
                // POSIX compatibility stub; do nothing
            } else if (!arg.empty() && arg[0] != '-') {
                options.username = arg;
            } else {
                std::cerr << "userdel: unrecognized option '" << arg << "'\n";
                std::cerr << "Try 'userdel --help' for more information.\n";
                return 2;
            }
        }

        // Automatic pipe detection or explicit --pipe flag
        if (options.pipeMode || (options.username.empty() && InputPipeline::IsPipeActive())) {
            return InputPipeline::ProcessBatchPipe(options);
        }

        if (options.username.empty()) {
            std::cerr << "userdel: error: no username specified. Use --help for usage.\n";
            return 2;
        }

        if (options.verbose) {
            std::cout << "Target User: " << options.username << "\n";
            std::cout << "Remove Profile/Home: " << (options.removeHomeAndProfile ? "Yes" : "No") << "\n";
            std::cout << "Force Kill Processes: " << (options.force ? "Yes" : "No") << "\n";
        }

        std::string err;
        if (!WindowsUserManager::DeleteAccount(options, err)) {
            std::cerr << "userdel: " << err << "\n";
            return 1;
        }

        if (options.verbose) {
            std::cout << "userdel: user '" << options.username << "' deleted successfully.\n";
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