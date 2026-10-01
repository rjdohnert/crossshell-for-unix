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

    // Helper for string manipulations and UTF-8 <-> UTF-16 conversions
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

        static std::vector<std::string> SplitDelimited(const std::string& str, const std::string& delims = ",;\t") {
            std::vector<std::string> tokens;
            size_t start = 0, end = 0;
            while ((end = str.find_first_of(delims, start)) != std::string::npos) {
                tokens.emplace_back(Trim(str.substr(start, end - start)));
                start = end + 1;
            }
            tokens.emplace_back(Trim(str.substr(start)));
            return tokens;
        }
    };

    // Configuration / Argument options model
    struct CommandOptions {
        std::string groupName;                  // Target group to modify
        std::optional<std::string> newName;     // -n, --new-name NEW_GROUP
        std::optional<std::string> comment;     // -c, --comment COMMENT
        bool force = false;                     // -f, --force
        bool quiet = false;                     // -q, --quiet
        bool verbose = false;                   // -v, --verbose
        bool showHelp = false;                  // -h, --help, /?
        bool showVersion = false;               // -V, --version
        bool readFromStdin = false;             // Read modifications from pipeline / STDIN
    };

    // Windows Local Group Modification Manager
    class WindowsGroupManager {
    public:
        struct Result {
            bool success;
            unsigned long errorCode;
            std::string message;
        };

        static Result ModifyLocalGroup(
            const std::string& currentName,
            const std::optional<std::string>& newName,
            const std::optional<std::string>& comment,
            bool force
        ) {
            if (currentName.empty()) {
                return { false, ERROR_INVALID_PARAMETER, "Group name cannot be empty." };
            }

            if (!newName.has_value() && !comment.has_value()) {
                return { false, ERROR_INVALID_PARAMETER, "No modifications specified (use -n or -c)." };
            }

            std::wstring wCurrentName = StringConverter::ToWide(currentName);
            DWORD paramErr = 0;

            // Step 1: Modify Comment if specified
            if (comment.has_value()) {
                std::wstring wComment = StringConverter::ToWide(comment.value());
                LOCALGROUP_INFO_1002 lgi1002;
                lgi1002.lgrpi1002_comment = wComment.data();

                NET_API_STATUS status = NetLocalGroupSetInfo(
                    nullptr,
                    wCurrentName.c_str(),
                    1002,
                    reinterpret_cast<LPBYTE>(&lgi1002),
                    &paramErr
                );

                if (status != NERR_Success) {
                    return HandleApiError(status, force, "comment update");
                }
            }

            // Step 2: Rename group if specified
            if (newName.has_value()) {
                if (newName.value() == currentName) {
                    if (!force) {
                        return { true, 0, "Target name matches current name. No rename needed." };
                    }
                } else {
                    std::wstring wNewName = StringConverter::ToWide(newName.value());
                    LOCALGROUP_INFO_0 lgi0;
                    lgi0.lgrpi0_name = wNewName.data();

                    NET_API_STATUS status = NetLocalGroupSetInfo(
                        nullptr,
                        wCurrentName.c_str(),
                        0,
                        reinterpret_cast<LPBYTE>(&lgi0),
                        &paramErr
                    );

                    if (status != NERR_Success) {
                        return HandleApiError(status, force, "rename");
                    }
                }
            }

            return { true, 0, "Group modified successfully." };
        }

    private:
        static Result HandleApiError(DWORD status, bool force, std::string_view operation) {
            if ((status == NERR_GroupNotFound || status == ERROR_NO_SUCH_ALIAS) && force) {
                return { true, status, "Group does not exist (force ignored error)." };
            }
            if (status == NERR_GroupExists && force) {
                return { true, status, "New group name already exists (force ignored error)." };
            }
            return { false, status, ResolveErrorMessage(status) + " during " + std::string(operation) + "." };
        }

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
            case NERR_GroupExists:
                return "A group with the specified new name already exists.";
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
                } else if (arg == "-n" || arg == "--new-name") {
                    if (i + 1 < argc) {
                        options.newName = argv[++i];
                    } else {
                        std::cerr << "groupmod: option '" << arg << "' requires an argument\n";
                    }
                } else if (arg == "-c" || arg == "--comment") {
                    if (i + 1 < argc) {
                        options.comment = argv[++i];
                    } else {
                        std::cerr << "groupmod: option '" << arg << "' requires an argument\n";
                    }
                } else if (arg == "-") {
                    options.readFromStdin = true;
                } else if (!arg.empty() && arg[0] == '-') {
                    std::cerr << "groupmod: unrecognized option '" << arg << "'\n";
                    std::cerr << "Try 'groupmod --help' for more information.\n";
                } else {
                    if (options.groupName.empty()) {
                        options.groupName = arg;
                    } else {
                        std::cerr << "groupmod: extra operand '" << arg << "'\n";
                    }
                }
            }

            // Detect if stdin has redirected/piped input
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
    class GroupModApplication {
    private:
        CommandOptions m_options;

    public:
        explicit GroupModApplication(CommandOptions options) : m_options(std::move(options)) {}

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
                std::cerr << "groupmod: missing operand\n";
                std::cerr << "Try 'groupmod --help' for more information.\n";
                return 2; // Invalid command syntax
            }

            if (!m_options.newName.has_value() && !m_options.comment.has_value()) {
                std::cerr << "groupmod: no changes specified. Use -n <new-name> or -c <comment>.\n";
                std::cerr << "Try 'groupmod --help' for more information.\n";
                return 2;
            }

            return ProcessSingleModification(m_options.groupName, m_options.newName, m_options.comment);
        }

    private:
        int ProcessSingleModification(
            const std::string& currentName,
            const std::optional<std::string>& newName,
            const std::optional<std::string>& comment
        ) {
            if (m_options.verbose) {
                std::cout << "[INFO] Modifying group '" << currentName << "'...";
                if (newName.has_value()) std::cout << " [New Name: " << newName.value() << "]";
                if (comment.has_value()) std::cout << " [Comment: \"" << comment.value() << "\"]";
                std::cout << std::endl;
            }

            auto result = WindowsGroupManager::ModifyLocalGroup(currentName, newName, comment, m_options.force);

            if (!result.success) {
                std::cerr << "groupmod: cannot modify group '" << currentName << "': " << result.message << "\n";
                if (result.errorCode == NERR_GroupNotFound || result.errorCode == ERROR_NO_SUCH_ALIAS) {
                    return 6; // Group does not exist
                }
                if (result.errorCode == NERR_GroupExists) {
                    return 9; // Group name already in use
                }
                if (result.errorCode == ERROR_ACCESS_DENIED) {
                    return 10; // Permission denied
                }
                return 1;
            }

            if (!m_options.quiet) {
                if (m_options.verbose) {
                    std::cout << "[SUCCESS] " << result.message << " (" << currentName << ")" << std::endl;
                }
            }
            return 0;
        }

        int ProcessPipeline() {
            std::string line;
            int overallStatus = 0;
            size_t count = 0;

            if (m_options.verbose) {
                std::cout << "[INFO] Reading modifications from standard input pipeline...\n";
            }

            while (std::getline(std::cin, line)) {
                std::string trimmed = StringConverter::Trim(line);
                if (trimmed.empty() || trimmed[0] == '#') {
                    continue; // Skip comments and empty lines
                }

                // Supported Formats:
                // 1. CSV/TSV: OldName,NewName,Comment
                // 2. CSV/TSV: OldName,NewName
                // 3. Plain:   OldName (uses -n and/or -c options passed via CLI)
                auto tokens = StringConverter::SplitDelimited(trimmed);
                std::string group = tokens[0];
                std::optional<std::string> newName = m_options.newName;
                std::optional<std::string> comment = m_options.comment;

                if (tokens.size() == 2) {
                    newName = tokens[1];
                } else if (tokens.size() >= 3) {
                    if (!tokens[1].empty()) newName = tokens[1];
                    if (!tokens[2].empty()) comment = tokens[2];
                }

                int code = ProcessSingleModification(group, newName, comment);
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
            std::cout << "groupmod 2.4.0\n";
            std::cout << "Copyright (C) 2026 Roberto J Dohnert\n";
            std::cout << "License BSD-3 Clause: <https://opensource.org/licenses/BSD-3-Clause>\n";
        }

        void PrintHelp() const {
              std::cout << R"(groupmod(1)             CrossShell for UNIX Reference Manual                  groupmod(1)

    NAME
        groupmod - modify a local user group on Windows

    SYNOPSIS
        groupmod [OPTIONS] GROUP
        type MODS.txt | groupmod [OPTIONS]
        Get-Content MODS.txt | groupmod [OPTIONS]

    DESCRIPTION
        Modifies the definition of a local group by altering its entry in the
        Windows Security Account Manager (SAM). When no GROUP operand is supplied
        and standard input is redirected or piped, modification definitions are
        read line by line.

    OPTIONS
        -n, --new-name NEW_GROUP
            Rename GROUP to NEW_GROUP.

        -c, --comment COMMENT
            Set or update the local group's description.

        -f, --force
            Succeed if the group does not exist or is already named NEW_GROUP.

        -q, --quiet
            Suppress normal messages.

        -v, --verbose
            Display detailed step-by-step diagnostic information.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

        -V, --version
            Display version information and exit.

        -
            Explicitly read modification entries from standard input.

    PIPING AND STREAMING
        Input lines may contain OldGroup,NewGroup,Comment; OldGroup,NewGroup;
        or a single OldGroup to which command-line -n or -c options apply. Comment
        lines beginning with # are ignored.

    EXIT STATUS
        0          Success, or an ignorable condition with -f specified.
        2          Invalid command syntax or missing arguments.
        6          Specified group does not exist.
        9          New group name is already in use.
        10         Access denied or insufficient privileges.

    EXAMPLES
        groupmod -n SeniorEngineers Developers
            Rename a local group.

        groupmod -c "Core Cloud Infrastructure Team" DevOps
            Update a group description.

        groupmod -n SecOps -c "Security & Operations Team" Security
            Rename a group and update its description simultaneously.

        (echo QA,QualityAssurance & echo Test,Testing) | groupmod -v
            Pipe multiple renames from CMD.

        Get-Content .\group_updates.csv | .\groupmod.exe -v
            Pipe a batch modification file from PowerShell.

    REQUIREMENTS
        Run from an elevated terminal to modify the local Windows SAM security database.

    CrossShell for UNIX                                                    groupmod(1)
    )";
        }
    };
}

int main(int argc, char* argv[]) {
    try {
        auto options = Utility::CommandLineParser::Parse(argc, argv);
        Utility::GroupModApplication app(std::move(options));
        return app.Run();
    } catch (const std::exception& ex) {
        std::cerr << "groupmod: fatal error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "groupmod: unknown fatal error occurred.\n";
        return 1;
    }
}