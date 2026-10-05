/*
BSD 3-Clause License

CrossShell for UNIX
Copyright (c) 2026, Roberto J Dohnert
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <lm.h>
#include <io.h>

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <sstream>
#include <memory>
#include <iomanip>

#pragma comment(lib, "netapi32.lib")

namespace Utility {

    // Helper for UTF-8 <-> UTF-16 conversions
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

    // Configuration / Argument options data model
    struct CommandOptions {
        std::string groupName;
        std::string comment;
        bool force = false;          // -f, --force: Exit successfully if the group exists
        bool systemGroup = false;     // -r, --system: Mark/designate as a system group
        bool quiet = false;           // -q, --quiet: Suppress non-error output
        bool verbose = false;         // -v, --verbose: Detailed execution logging
        bool showHelp = false;        // -h, --help
        bool showVersion = false;     // -V, --version
        bool readFromStdin = false;   // Read group names from pipeline / STDIN
    };

    // Windows Local Group abstraction
    class WindowsGroupManager {
    public:
        struct Result {
            bool success;
            unsigned long errorCode;
            std::string message;
        };

        static Result CreateLocalGroup(const std::string& groupName, const std::string& comment, bool force) {
            if (groupName.empty()) {
                return { false, ERROR_INVALID_PARAMETER, "Group name cannot be empty." };
            }

            std::wstring wGroupName = StringConverter::ToWide(groupName);
            std::wstring wComment = StringConverter::ToWide(comment);

            LOCALGROUP_INFO_1 groupInfo;
            groupInfo.lgrpi1_name = wGroupName.data();
            groupInfo.lgrpi1_comment = wComment.empty() ? nullptr : wComment.data();

            DWORD paramErr = 0;
            NET_API_STATUS status = NetLocalGroupAdd(
                nullptr,            // Local computer
                1,                  // Level 1 (Name + Comment)
                reinterpret_cast<LPBYTE>(&groupInfo),
                &paramErr
            );

            if (status == NERR_Success) {
                return { true, 0, "Group created successfully." };
            }

            if (status == NERR_GroupExists) {
                if (force) {
                    return { true, NERR_GroupExists, "Group already exists (force ignored error)." };
                }
                return { false, NERR_GroupExists, "Group already exists." };
            }

            return { false, status, ResolveErrorMessage(status) };
        }

    private:
        static std::string ResolveErrorMessage(DWORD errorCode) {
            switch (errorCode) {
            case ERROR_ACCESS_DENIED:
                return "Access denied. Administrator privileges (Elevated token) required.";
            case NERR_InvalidComputer:
                return "The computer name is invalid.";
            case NERR_NotPrimary:
                return "The operation is allowed only on the primary domain controller.";
            case NERR_GroupExists:
                return "The local group already exists.";
            case ERROR_INVALID_NAME:
                return "The specified group name contains invalid characters.";
            case ERROR_NO_SUCH_MEMBER:
                return "One or more of the specified members do not exist.";
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

    // Command line argument parser
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
                } else if (arg == "-f" || arg == "--force") {
                    options.force = true;
                } else if (arg == "-r" || arg == "--system") {
                    options.systemGroup = true;
                } else if (arg == "-q" || arg == "--quiet") {
                    options.quiet = true;
                } else if (arg == "-v" || arg == "--verbose") {
                    options.verbose = true;
                } else if (arg == "-c" || arg == "--comment") {
                    if (i + 1 < argc) {
                        options.comment = argv[++i];
                    } else {
                        std::cerr << "groupadd: option '" << arg << "' requires an argument\n";
                    }
                } else if (arg == "-") {
                    options.readFromStdin = true;
                } else if (!arg.empty() && arg[0] == '-') {
                    std::cerr << "groupadd: unrecognized option '" << arg << "'\n";
                    std::cerr << "Try 'groupadd --help' for more information.\n";
                } else {
                    if (options.groupName.empty()) {
                        options.groupName = arg;
                    } else {
                        std::cerr << "groupadd: extra operand '" << arg << "'\n";
                    }
                }
            }

            // Detect if stdin has redirected input / pipe
            if (options.groupName.empty() && !IsTerminalInput()) {
                options.readFromStdin = true;
            }

            return options;
        }

        static bool IsTerminalInput() {
            return _isatty(_fileno(stdin)) != 0;
        }
    };

    // Core application orchestrator
    class GroupAddApplication {
    private:
        CommandOptions m_options;

    public:
        explicit GroupAddApplication(CommandOptions options) : m_options(std::move(options)) {}

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

            if (m_options.groupName.empty()) {
                std::cerr << "groupadd: missing operand\n";
                std::cerr << "Try 'groupadd --help' for more information.\n";
                return 2; // Invalid usage
            }

            return ProcessSingleGroup(m_options.groupName, m_options.comment);
        }

    private:
        int ProcessSingleGroup(const std::string& name, const std::string& comment) {
            if (m_options.verbose) {
                std::cout << "[INFO] Creating local group: " << name;
                if (!comment.empty()) {
                    std::cout << " (Comment: \"" << comment << "\")";
                }
                std::cout << "..." << std::endl;
            }

            auto result = WindowsGroupManager::CreateLocalGroup(name, comment, m_options.force);

            if (!result.success) {
                std::cerr << "groupadd: cannot create group '" << name << "': " << result.message << "\n";
                return (result.errorCode == NERR_GroupExists) ? 9 : 4; 
            }

            if (!m_options.quiet && (!m_options.force || result.errorCode != NERR_GroupExists)) {
                if (m_options.verbose) {
                    std::cout << "[SUCCESS] " << result.message << std::endl;
                }
            }
            return 0;
        }

        int ProcessPipeline() {
            std::string line;
            int overallStatus = 0;
            size_t count = 0;

            if (m_options.verbose) {
                std::cout << "[INFO] Reading group names from standard input pipeline...\n";
            }

            while (std::getline(std::cin, line)) {
                std::string trimmed = StringConverter::Trim(line);
                if (trimmed.empty() || trimmed[0] == '#') {
                    continue; // Skip blank lines and comments
                }

                // Support CSV/TSV format: "GroupName,Comment" or "GroupName"
                std::string group = trimmed;
                std::string comment = m_options.comment;

                size_t commaPos = trimmed.find_first_of(",;\t");
                if (commaPos != std::string::npos) {
                    group = StringConverter::Trim(trimmed.substr(0, commaPos));
                    comment = StringConverter::Trim(trimmed.substr(commaPos + 1));
                }

                int code = ProcessSingleGroup(group, comment);
                if (code != 0 && overallStatus == 0) {
                    overallStatus = code;
                }
                count++;
            }

            if (m_options.verbose) {
                std::cout << "[INFO] Pipeline completed. Total processed: " << count << "\n";
            }

            return overallStatus;
        }

        void PrintVersion() const {
            std::cout << "groupadd 2.5.0\n";
            std::cout << "Copyright (C) 2026 Roberto J Dohnert.\n";
            std::cout << "License BSD-3 Clause: <https://opensource.org/licenses/BSD-3-Clause>\n";
        }

        void PrintHelp() const {
              std::cout << R"(groupadd(1)             CrossShell for UNIX Reference Manual                  groupadd(1)

    NAME
        groupadd - create a new local user group on Windows

    SYNOPSIS
        groupadd [OPTIONS] GROUP
        type GROUPS.txt | groupadd [OPTIONS]
        Get-Content GROUPS.txt | groupadd [OPTIONS]

    DESCRIPTION
        Creates a local security group account using the Windows Security Account
        Manager (SAM) and NetLocalGroup APIs. When no group operand is supplied and
        standard input is redirected or piped, group names are read line by line.

    OPTIONS
        -c, --comment COMMENT
            Assign a description to the local group. With piped input, this is the
            default comment unless a comma or tab delimiter supplies one.

        -f, --force
            Exit successfully if the group already exists.

        -r, --system
            Designate a system management group; retained for UNIX compatibility.

        -q, --quiet
            Suppress non-critical warnings and standard messages.

        -v, --verbose
            Display detailed step-by-step diagnostic information.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

        -
            Explicitly read group names from standard input.

    PIPING AND REDIRECTION
        Input may be supplied as a plain group name, as CSV or TSV containing a
        group name and optional description, or as a comment line beginning with #.

    EXIT STATUS
        0          Success, or group exists when -f is specified.
        2          Invalid command syntax or missing arguments.
        4          Windows system error or access denied.
        9          Group already exists when -f is not specified.

    EXAMPLES
        groupadd Developers
            Create a basic local group.

        groupadd -c "Core Engineering Team" Engineers
            Create a local group with a description.

        groupadd -f -c "Docker Admins" docker-users
            Create a group if it does not already exist.

        (echo QA & echo DevOps & echo Managers) | groupadd -f
            Pipe multiple groups from CMD.

        Get-Content .\groups.csv | .\groupadd.exe -v
            Pipe group definitions from PowerShell.

    REQUIREMENTS
        Run from an elevated terminal to modify the local Windows SAM security store.

    CrossShell for UNIX                                                    groupadd(1)
    )";
        }
    };
}

int main(int argc, char* argv[]) {
    try {
        auto options = Utility::CommandLineParser::Parse(argc, argv);
        Utility::GroupAddApplication app(std::move(options));
        return app.Run();
    } catch (const std::exception& ex) {
        std::cerr << "groupadd: fatal error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "groupadd: unknown fatal error occurred.\n";
        return 1;
    }
}