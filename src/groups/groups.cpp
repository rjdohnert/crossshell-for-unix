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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <lm.h>
#include <sddl.h>
#include <io.h>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <optional>
#include <sstream>
#include <memory>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "netapi32.lib")
#pragma comment(lib, "advapi32.lib")

namespace Utility {

    // String manipulation and UTF-8 <-> UTF-16 conversions
    class StringConverter {
    public:
        static std::wstring ToWide(std::string_view str) {
            if (str.empty()) return L"";
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
            std::wstring wstr(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstr[0], size_needed);
            return wstr;
        }

        static std::string ToNarrow(std::wstring_view wstr) {
            if (wstr.empty()) return "";
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
            std::string str(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), &str[0], size_needed, nullptr, nullptr);
            return str;
        }

        static std::string Trim(const std::string& str) {
            const size_t first = str.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return "";
            const size_t last = str.find_last_not_of(" \t\r\n");
            return str.substr(first, (last - first + 1));
        }
    };

    // Data model for a group entity
    struct GroupInfo {
        std::string name;
        std::string sid;
        bool isLocal = true;

        bool operator<(const GroupInfo& other) const {
            return name < other.name;
        }
    };

    // Command-line configuration options
    struct CommandOptions {
        std::vector<std::string> usernames;
        std::string delimiter = " ";      // Default delimiter between group names
        bool showSid = false;             // -s, --sid: Show SID alongside group name
        bool showType = false;            // -t, --type: Distinguish [Local] vs [Global]
        bool quiet = false;               // -q, --quiet: Suppress non-error output
        bool verbose = false;             // -v, --verbose: Detailed execution logging
        bool showHelp = false;            // -h, --help, /?
        bool showVersion = false;         // -V, --version
        bool readFromStdin = false;       // Read usernames from pipeline / STDIN
    };

    // Windows User & Group Query Manager
    class WindowsGroupQuery {
    public:
        struct QueryResult {
            bool success;
            unsigned long errorCode;
            std::string message;
            std::vector<GroupInfo> groups;
        };

        static std::string GetCurrentUsername() {
            WCHAR buffer[UNLEN + 1];
            DWORD size = UNLEN + 1;
            if (GetUserNameW(buffer, &size)) {
                return StringConverter::ToNarrow(buffer);
            }
            return "UNKNOWN_USER";
        }

        static QueryResult QueryUserGroups(const std::string& username, bool fetchSids) {
            if (username.empty()) {
                return { false, ERROR_INVALID_PARAMETER, "Username cannot be empty.", {} };
            }

            std::wstring wUsername = StringConverter::ToWide(username);
            std::set<GroupInfo> collectedGroups;

            // 1. Query Local Groups (including indirect memberships via global groups)
            LPLOCALGROUP_USERS_INFO_0 pLocalBuf = nullptr;
            DWORD localEntriesRead = 0;
            DWORD localTotalEntries = 0;

            NET_API_STATUS status = NetUserGetLocalGroups(
                nullptr,
                wUsername.c_str(),
                0,
                LG_INCLUDE_INDIRECT,
                reinterpret_cast<LPBYTE*>(&pLocalBuf),
                MAX_PREFERRED_LENGTH,
                &localEntriesRead,
                &localTotalEntries
            );

            if (status == NERR_UserNotFound) {
                return { false, status, "no such user", {} };
            }

            if (status != NERR_Success && status != ERROR_MORE_DATA) {
                return { false, status, ResolveErrorMessage(status), {} };
            }

            if (pLocalBuf != nullptr) {
                for (DWORD i = 0; i < localEntriesRead; ++i) {
                    std::string gName = StringConverter::ToNarrow(pLocalBuf[i].lgrui0_name);
                    std::string sidStr = fetchSids ? LookupGroupSid(pLocalBuf[i].lgrui0_name) : "";
                    collectedGroups.insert(GroupInfo{ gName, sidStr, true });
                }
                NetApiBufferFree(pLocalBuf);
            }

            // 2. Query Global Groups
            LPGROUP_USERS_INFO_0 pGlobalBuf = nullptr;
            DWORD globalEntriesRead = 0;
            DWORD globalTotalEntries = 0;

            status = NetUserGetGroups(
                nullptr,
                wUsername.c_str(),
                0,
                reinterpret_cast<LPBYTE*>(&pGlobalBuf),
                MAX_PREFERRED_LENGTH,
                &globalEntriesRead,
                &globalTotalEntries
            );

            if (status == NERR_Success && pGlobalBuf != nullptr) {
                for (DWORD i = 0; i < globalEntriesRead; ++i) {
                    std::string gName = StringConverter::ToNarrow(pGlobalBuf[i].grui0_name);
                    std::string sidStr = fetchSids ? LookupGroupSid(pGlobalBuf[i].grui0_name) : "";
                    collectedGroups.insert(GroupInfo{ gName, sidStr, false });
                }
                NetApiBufferFree(pGlobalBuf);
            }

            std::vector<GroupInfo> result(collectedGroups.begin(), collectedGroups.end());
            return { true, 0, "Success", std::move(result) };
        }

    private:
        static std::string LookupGroupSid(LPCWSTR groupName) {
            DWORD cbSid = 0;
            DWORD cchDomain = 0;
            SID_NAME_USE peUse;

            LookupAccountNameW(nullptr, groupName, nullptr, &cbSid, nullptr, &cchDomain, &peUse);
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                return "";
            }

            std::vector<BYTE> sidBuffer(cbSid);
            std::vector<WCHAR> domainBuffer(cchDomain);
            PSID pSid = reinterpret_cast<PSID>(sidBuffer.data());

            if (LookupAccountNameW(nullptr, groupName, pSid, &cbSid, domainBuffer.data(), &cchDomain, &peUse)) {
                LPWSTR stringSid = nullptr;
                if (ConvertSidToStringSidW(pSid, &stringSid)) {
                    std::string result = StringConverter::ToNarrow(stringSid);
                    LocalFree(stringSid);
                    return result;
                }
            }
            return "";
        }

        static std::string ResolveErrorMessage(DWORD errorCode) {
            switch (errorCode) {
            case ERROR_ACCESS_DENIED:
                return "Access denied. Insufficient permissions.";
            case NERR_InvalidComputer:
                return "The computer name is invalid.";
            case NERR_UserNotFound:
                return "no such user";
            default: {
                LPWSTR buffer = nullptr;
                size_t size = FormatMessageW(
                    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    reinterpret_cast<LPWSTR>(&buffer), 0, nullptr
                );
                std::string msg = size ? StringConverter::ToNarrow(buffer) : "Unknown Windows API error.";
                if (buffer) LocalFree(buffer);
                return StringConverter::Trim(msg);
            }
            }
        }
    };

    // Command-line argument parser
    class CommandLineParser {
    public:
        static CommandOptions Parse(int argc, char* argv[]) {
            CommandOptions options;

            for (int i = 1; i < argc; ++i) {
                std::string_view arg = argv[i];

                if (arg == "-h" || arg == "--help" || arg == "/?") {
                    options.showHelp = true;
                    return options;
                } else if (arg == "-V" || arg == "--version") {
                    options.showVersion = true;
                    return options;
                } else if (arg == "-s" || arg == "--sid") {
                    options.showSid = true;
                } else if (arg == "-t" || arg == "--type") {
                    options.showType = true;
                } else if (arg == "-q" || arg == "--quiet") {
                    options.quiet = true;
                } else if (arg == "-v" || arg == "--verbose") {
                    options.verbose = true;
                } else if (arg == "-d" || arg == "--delimiter") {
                    if (i + 1 < argc) {
                        options.delimiter = argv[++i];
                        if (options.delimiter == "\\n") options.delimiter = "\n";
                        if (options.delimiter == "\\t") options.delimiter = "\t";
                    } else {
                        std::cerr << "groups: option '" << arg << "' requires an argument\n";
                    }
                } else if (arg == "-") {
                    options.readFromStdin = true;
                } else if (!arg.empty() && arg[0] == '-') {
                    std::cerr << "groups: unrecognized option '" << arg << "'\n";
                    std::cerr << "Try 'groups --help' for more information.\n";
                } else {
                    options.usernames.emplace_back(arg);
                }
            }

            // If no usernames provided and standard input is redirected/piped
            if (options.usernames.empty() && !IsTerminalInput()) {
                options.readFromStdin = true;
            }

            return options;
        }

        static bool IsTerminalInput() {
            return _isatty(_fileno(stdin)) != 0;
        }
    };

    // Application orchestrator
    class GroupsApplication {
    private:
        CommandOptions m_options;

    public:
        explicit GroupsApplication(CommandOptions options) : m_options(std::move(options)) {}

        int Run() {
            if (m_options.showHelp) {
                PrintHelp();
                return 0;
            }

            if (m_options.showVersion) {
                PrintVersion();
                return 0;
            }

            if (m_options.readFromStdin) {
                return ProcessPipeline();
            }

            // Default behavior: if no usernames are specified, query the current user
            if (m_options.usernames.empty()) {
                std::string currentUser = WindowsGroupQuery::GetCurrentUsername();
                return ProcessSingleUser(currentUser, false);
            }

            int overallStatus = 0;
            bool printPrefix = (m_options.usernames.size() > 1);

            for (const auto& user : m_options.usernames) {
                int code = ProcessSingleUser(user, printPrefix);
                if (code != 0 && overallStatus == 0) {
                    overallStatus = code;
                }
            }

            return overallStatus;
        }

    private:
        int ProcessSingleUser(const std::string& username, bool printPrefix) {
            if (m_options.verbose) {
                std::cerr << "[INFO] Querying group memberships for user: " << username << "...\n";
            }

            auto result = WindowsGroupQuery::QueryUserGroups(username, m_options.showSid);

            if (!result.success) {
                std::cerr << "groups: '" << username << "': " << result.message << "\n";
                return (result.errorCode == NERR_UserNotFound) ? 1 : 2;
            }

            if (printPrefix) {
                std::cout << username << " : ";
            }

            for (size_t i = 0; i < result.groups.size(); ++i) {
                const auto& group = result.groups[i];

                std::cout << group.name;

                if (m_options.showType) {
                    std::cout << (group.isLocal ? "[Local]" : "[Global]");
                }

                if (m_options.showSid && !group.sid.empty()) {
                    std::cout << "(" << group.sid << ")";
                }

                if (i + 1 < result.groups.size()) {
                    std::cout << m_options.delimiter;
                }
            }

            std::cout << "\n";
            return 0;
        }

        int ProcessPipeline() {
            std::string line;
            int overallStatus = 0;

            if (m_options.verbose) {
                std::cerr << "[INFO] Reading usernames from standard input pipeline...\n";
            }

            while (std::getline(std::cin, line)) {
                std::string trimmed = StringConverter::Trim(line);
                if (trimmed.empty() || trimmed[0] == '#') {
                    continue; // Skip comments and empty lines
                }

                // If formatted as CSV/TSV, take the first column
                size_t delimPos = trimmed.find_first_of(",;\t");
                if (delimPos != std::string::npos) {
                    trimmed = StringConverter::Trim(trimmed.substr(0, delimPos));
                }

                int code = ProcessSingleUser(trimmed, true);
                if (code != 0 && overallStatus == 0) {
                    overallStatus = code;
                }
            }

            return overallStatus;
        }

        void PrintVersion() const {
            std::cout << "groups 2.4.0\n";
            std::cout << "Copyright (C) 2026 Roberto J Dohnert\n";
            std::cout << "License BSD-3 Clause: <https://opensource.org/licenses/BSD-3-Clause>\n";
        }

        void PrintHelp() const {
            std::cout << R"(groups(1)               CrossShell for UNIX Reference Manual                groups(1)

    NAME
        groups - print the groups a user is in on Windows

    SYNOPSIS
        groups [OPTIONS] [USERNAME...]
        type USERS.txt | groups [OPTIONS]

    DESCRIPTION
        The groups utility displays the names of the local and global security
        groups of which each specified USERNAME is a member.

        If no USERNAME is specified and standard input is attached to a terminal,
        the group memberships of the current logged-in process/user are printed.

    OPTIONS
        -d, --delimiter DELIM
            Use DELIM as output delimiter instead of default single space.

        -s, --sid
            Lookup and display the Security Identifier (SID) for each group.

        -t, --type
            Display the group scope tag: [Local] or [Global].

        -q, --quiet
            Quiet mode. Suppress non-critical warnings.

        -v, --verbose
            Verbose mode. Displays diagnostic query information to standard error.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Output version information and exit.

    EXAMPLES
        groups
            Display group memberships for the current user.

        groups Administrator --sid
            Display groups and SIDs for the Administrator account.

    CrossShell for UNIX                                                 groups(1)
)";
        }
    };
}

int main(int argc, char* argv[]) {
    try {
        auto options = Utility::CommandLineParser::Parse(argc, argv);
        Utility::GroupsApplication app(std::move(options));
        return app.Run();
    } catch (const std::exception& ex) {
        std::cerr << "groups: fatal error: " << ex.what() << "\n";
        return 2;
    } catch (...) {
        std::cerr << "groups: unknown fatal error occurred.\n";
        return 2;
    }
}