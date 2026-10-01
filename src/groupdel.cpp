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

    // Helper for string operations and UTF-8 <-> UTF-16 conversions
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

    // Command-line options model
    struct CommandOptions {
        std::vector<std::string> groupNames;
        bool force = false;          // -f, --force: Suppress error if group doesn't exist
        bool quiet = false;          // -q, --quiet: Suppress non-error output
        bool verbose = false;        // -v, --verbose: Detailed execution logging
        bool showHelp = false;       // -h, --help, /?
        bool showVersion = false;    // -V, --version
        bool readFromStdin = false;  // Read group names from pipeline / STDIN
    };

    // Windows Local Group Deletion Manager
    class WindowsGroupManager {
    public:
        struct Result {
            bool success;
            unsigned long errorCode;
            std::string message;
        };

        static Result DeleteLocalGroup(const std::string& groupName, bool force) {
            if (groupName.empty()) {
                return { false, ERROR_INVALID_PARAMETER, "Group name cannot be empty." };
            }

            std::wstring wGroupName = StringConverter::ToWide(groupName);

            NET_API_STATUS status = NetLocalGroupDel(
                nullptr,              // Local computer
                wGroupName.c_str()    // Group name to delete
            );

            if (status == NERR_Success) {
                return { true, 0, "Group deleted successfully." };
            }

            if (status == NERR_GroupNotFound || status == ERROR_NO_SUCH_ALIAS) {
                if (force) {
                    return { true, status, "Group does not exist (force ignored error)." };
                }
                return { false, status, "Group does not exist." };
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
            case NERR_GroupNotFound:
            case ERROR_NO_SUCH_ALIAS:
                return "The specified local group does not exist.";
            case ERROR_INVALID_NAME:
                return "The specified group name contains invalid characters.";
            case NERR_SpeGroupOp:
                return "Cannot perform this operation on built-in or special system groups.";
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
                } else if (arg == "-f" || arg == "--force") {
                    options.force = true;
                } else if (arg == "-q" || arg == "--quiet") {
                    options.quiet = true;
                } else if (arg == "-v" || arg == "--verbose") {
                    options.verbose = true;
                } else if (arg == "-") {
                    options.readFromStdin = true;
                } else if (!arg.empty() && arg[0] == '-') {
                    std::cerr << "groupdel: unrecognized option '" << arg << "'\n";
                    std::cerr << "Try 'groupdel --help' for more information.\n";
                } else {
                    options.groupNames.emplace_back(arg);
                }
            }

            // Detect if stdin has redirected/piped input
            if (options.groupNames.empty() && !IsTerminalInput()) {
                options.readFromStdin = true;
            }

            return options;
        }

        static bool IsTerminalInput() {
            return _isatty(_fileno(stdin)) != 0;
        }
    };

    // Core application orchestrator
    class GroupDelApplication {
    private:
        CommandOptions m_options;

    public:
        explicit GroupDelApplication(CommandOptions options) : m_options(std::move(options)) {}

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

            if (m_options.groupNames.empty()) {
                std::cerr << "groupdel: missing operand\n";
                std::cerr << "Try 'groupdel --help' for more information.\n";
                return 2; // Invalid command syntax
            }

            int overallStatus = 0;
            for (const auto& group : m_options.groupNames) {
                int code = ProcessSingleGroup(group);
                if (code != 0 && overallStatus == 0) {
                    overallStatus = code;
                }
            }
            return overallStatus;
        }

    private:
        int ProcessSingleGroup(const std::string& name) {
            if (m_options.verbose) {
                std::cout << "[INFO] Deleting local group: " << name << "..." << std::endl;
            }

            auto result = WindowsGroupManager::DeleteLocalGroup(name, m_options.force);

            if (!result.success) {
                std::cerr << "groupdel: cannot remove group '" << name << "': " << result.message << "\n";
                if (result.errorCode == NERR_GroupNotFound || result.errorCode == ERROR_NO_SUCH_ALIAS) {
                    return 6; // Standard UNIX exit code: group does not exist
                }
                if (result.errorCode == ERROR_ACCESS_DENIED) {
                    return 10; // Standard UNIX exit code: cannot update group file / permission failure
                }
                if (result.errorCode == NERR_SpeGroupOp) {
                    return 8; // Cannot remove system/special group
                }
                return 1;
            }

            if (!m_options.quiet && (!m_options.force || (result.errorCode != NERR_GroupNotFound && result.errorCode != ERROR_NO_SUCH_ALIAS))) {
                if (m_options.verbose) {
                    std::cout << "[SUCCESS] " << result.message << " (" << name << ")" << std::endl;
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
                    continue; // Skip comments and empty lines
                }

                // If input line is formatted as CSV/TSV, extract the first column (group name)
                std::string group = trimmed;
                size_t delimPos = trimmed.find_first_of(",;\t");
                if (delimPos != std::string::npos) {
                    group = StringConverter::Trim(trimmed.substr(0, delimPos));
                }

                int code = ProcessSingleGroup(group);
                if (code != 0 && overallStatus == 0) {
                    overallStatus = code;
                }
                count++;
            }

            if (m_options.verbose) {
                std::cout << "[INFO] Pipeline processing completed. Total processed: " << count << "\n";
            }

            return overallStatus;
        }

        void PrintVersion() const {
            std::cout << "groupdel 2.4.0 \n";
            std::cout << "Copyright (C) 2026 Roberto J Dohnert.\n";
            std::cout << "License BSD-3 Clause: <https://opensource.org/licenses/BSD-3-Clause>\n";
        }

        void PrintHelp() const {
              std::cout << R"(groupdel(1)             CrossShell for UNIX Reference Manual                  groupdel(1)

    NAME
        groupdel - delete a local user group on Windows

    SYNOPSIS
        groupdel [OPTIONS] GROUP...
        type GROUPS.txt | groupdel [OPTIONS]
        Get-Content GROUPS.txt | groupdel [OPTIONS]

    DESCRIPTION
        Deletes entries for the specified local groups from the Windows Security
        Account Manager (SAM) database. When no group operand is supplied and
        standard input is redirected or piped, group names are read line by line.

    OPTIONS
        -f, --force
            Remove groups and suppress errors when a specified group does not exist.

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

    PIPING AND STREAMING
        Standard input accepts one group name per line, CSV or TSV with the group
        name in the first field, and comment lines beginning with #.

    EXIT STATUS
        0          Success, or group does not exist when -f is specified.
        2          Invalid command syntax or missing arguments.
        6          Specified group does not exist without -f.
        8          Built-in or special system group cannot be removed.
        10         Access denied or insufficient permissions.

    EXAMPLES
        groupdel Developers
            Delete a single local group.

        groupdel QA DevOps Managers
            Delete multiple local groups at once.

        groupdel -f OldDevGroup
            Delete a group safely in an idempotent automation script.

        (echo TempGroup1 & echo TempGroup2) | groupdel -f
            Pipe multiple groups from CMD.

        Get-Content .\deprecated_groups.txt | .\groupdel.exe -v
            Pipe groups from a text or CSV file in PowerShell.

    REQUIREMENTS
        Run from an elevated terminal to modify the Windows local security database.

    CrossShell for UNIX                                                    groupdel(1)
    )";
        }
    };
}

int main(int argc, char* argv[]) {
    try {
        auto options = Utility::CommandLineParser::Parse(argc, argv);
        Utility::GroupDelApplication app(std::move(options));
        return app.Run();
    } catch (const std::exception& ex) {
        std::cerr << "groupdel: fatal error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "groupdel: unknown fatal error occurred.\n";
        return 1;
    }
}