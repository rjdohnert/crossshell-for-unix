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
