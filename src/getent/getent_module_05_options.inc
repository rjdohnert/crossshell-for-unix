    // =========================================================================
    // CLI Argument Parser & Manual
    // =========================================================================
    struct Options {
        std::string database;
        std::vector<std::string> keys;
        bool read_stdin = false;
        bool show_help = false;
        bool show_version = false;
    };

    class ArgumentParser {
    public:
        static Options parse(int argc, char* argv[]) {
            Options opts;

            for (int i = 1; i < argc; ++i) {
                std::string_view arg = argv[i];

                if (arg == "-h" || arg == "--help" || arg == "/?") {
                    opts.show_help = true;
                    return opts;
                }
                if (arg == "-v" || arg == "-V" || arg == "--version") {
                    opts.show_version = true;
                    return opts;
                }
                if (opts.database.empty() && arg[0] != '-') {
                    opts.database = arg;
                } else if (arg == "-") {
                    opts.read_stdin = true;
                } else {
                    opts.keys.emplace_back(arg);
                }
            }

            return opts;
        }

        static void print_help(std::ostream& out, const DatabaseRegistry& /*reg*/) {
            out << R"(getent(1)           CrossShell for UNIX Reference Manual                getent(1)

    NAME
        getent - get entries from administrative databases

    SYNOPSIS
        getent [OPTIONS] DATABASE [KEY...]

    DESCRIPTION
        getent retrieves records from Name Service Switch (NSS) administrative
        databases. On Windows, native Win32 security, networking, and system driver
        stores are mapped to standard POSIX NSS databases.

    OPTIONS
        -h, --help
            Display this reference manual.

        -v, -V, --version
            Display version and license information.

        -
            Read query keys directly from standard input.

    AVAILABLE DATABASES
        passwd
            Local user accounts and security identifiers.

        group
            Local security groups and member accounts.

        hosts
            Static host mappings and DNS resolution records.

        services
            Internet network services and port mappings.

        protocols
            Internet network protocol numbers and aliases.

        networks
            Network adapter configurations and subnet definitions.

    EXAMPLES
        getent passwd
            Enumerate all user accounts in standard passwd format.

        getent passwd Administrator
            Query specific user account details.

        getent group Administrators
            Query specific local group details.

        getent hosts localhost
            Resolve host mapping for localhost.

        getent services 80/tcp
            Query service information for port 80 over TCP.

    CrossShell for UNIX                                                   getent(1)
)";
        }

        static void print_version(std::ostream& out) {
            out << "getent 2.9.0\n"
                << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }
    };

