    // =========================================================================
    // CLI Argument Parser & Manual
    // =========================================================================
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
                if (arg == "-c" || arg == "--omit-header") {
                    opts.omit_header = true;
                }
                else if (arg == "-a" || arg == "--access") {
                    opts.show_access = true;
                    opts.show_default = false;
                }
                else if (arg == "-d" || arg == "--default") {
                    opts.show_default = true;
                    opts.show_access = false;
                }
                else if (arg == "-n" || arg == "--numeric") {
                    opts.numeric_ids = true;
                }
                else if (arg == "-s" || arg == "--skip-base") {
                    opts.skip_base_only = true;
                }
                else if (arg == "-R" || arg == "--recursive") {
                    opts.recursive = true;
                }
                else if (arg == "-L" || arg == "--logical") {
                    opts.follow_symlinks = true;
                }
                else if (arg == "-P" || arg == "--physical") {
                    opts.follow_symlinks = false;
                }
                else if (arg == "-t" || arg == "--tabular") {
                    opts.tabular = true;
                }
                else if (arg == "-") {
                    opts.read_stdin = true;
                }
                else if (arg.size() > 1 && arg[0] == '-' && arg[1] != '-') {
                    // Bundled flags: e.g. -Rnc
                    for (size_t j = 1; j < arg.size(); ++j) {
                        char flag = arg[j];
                        switch (flag) {
                            case 'c': opts.omit_header = true; break;
                            case 'a': opts.show_access = true; opts.show_default = false; break;
                            case 'd': opts.show_default = true; opts.show_access = false; break;
                            case 'n': opts.numeric_ids = true; break;
                            case 's': opts.skip_base_only = true; break;
                            case 'R': opts.recursive = true; break;
                            case 'L': opts.follow_symlinks = true; break;
                            case 'P': opts.follow_symlinks = false; break;
                            case 't': opts.tabular = true; break;
                            default:
                                throw std::runtime_error(std::string("Unknown flag: -") + flag);
                        }
                    }
                }
                else {
                    opts.paths.emplace_back(arg);
                }
            }

            return opts;
        }

        static void print_help(std::ostream& out) {
            out << R"(getfacl(1)          CrossShell for UNIX Reference Manual                getfacl(1)

    NAME
        getfacl - get file access control lists

    SYNOPSIS
        getfacl [OPTIONS] FILE...

    DESCRIPTION
        getfacl displays the file name, owner, group, and Access Control Lists
        (ACLs) of files and directories on NTFS file systems. It maps Windows
        Security Descriptors, SIDs, and Discretionary Access Control Lists (DACLs)
        into standard POSIX ACL output notation.

    OPTIONS
        -a, --access
            Display the file access control list only.

        -d, --default
            Display the default and inherited directory access control list.

        -c, --omit-header
            Do not display the comment header.

        -n, --numeric
            Print numeric user and group security identifiers (SIDs).

        -s, --skip-base
            Skip files that have only standard/base access entries.

        -R, --recursive
            List the ACLs of all files and directories recursively.

        -L, --logical
            Follow symbolic links during recursion.

        -P, --physical
            Do not follow symbolic links (default).

        -t, --tabular
            Display output in an aligned tabular overview.

        -
            Read the list of target file paths from standard input.

        -h, --help
            Display this reference manual.

        -v, -V, --version
            Display version and license information.

    EXAMPLES
        getfacl file.txt
            Display ACLs for file.txt.

        getfacl -R -c C:\Projects > permissions_audit.txt
            Recursively list ACLs omitting comment headers.

        dir /B | getfacl -
            Read file paths from standard input.

    CrossShell for UNIX                                                   getfacl(1)
)";
        }

        static void print_version(std::ostream& out) {
            out << "getfacl 3.0.0\n"
                << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }
    };

