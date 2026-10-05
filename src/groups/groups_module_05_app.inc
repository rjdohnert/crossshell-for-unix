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
