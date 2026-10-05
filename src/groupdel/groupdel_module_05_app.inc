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
