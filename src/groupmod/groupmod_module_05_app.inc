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
