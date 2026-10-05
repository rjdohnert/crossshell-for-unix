    // =========================================================================
    // Help & Information Subsystem
    // =========================================================================
    class HelpSystem {
    public:
        static void print_version() {
            std::cout << "du version 5.1.0\n"
                      << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }

        static void print_help() {
            std::cout << R"(du(1)                     CrossShell for UNIX Reference Manual                     du(1)

    NAME
        du - estimate file and directory space usage

    SYNOPSIS
        du [OPTIONS] [FILE]...

    DESCRIPTION
        Reports Windows file and directory sizes using block, byte, or
        human-readable units. With no FILE specified, the current directory
        is scanned recursively.

        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -a, --all
            Include individual files, not only directory totals.

        -s, --summarize
            Display only a total summary for each argument.

        -c, --total
            Produce a cumulative grand total.

        -b, --apparent-size
            Print apparent sizes rather than disk allocation usage.

        -0, --null
            End each output line with a NUL character instead of newline.

        -L, --dereference
            Follow all symbolic links and reparse points.

        -d, --max-depth=N
            Print directory total only if N or fewer levels below argument.

        -k, --kilobytes
            Use 1024-byte blocks (default).

        -m, --megabytes
            Use 1048576-byte (1 MB) blocks.

        -H, --human-readable
            Print sizes in human-readable format (e.g., 1K, 234M, 2G).

        --exclude=PATTERN
            Exclude files and directories matching PATTERN.

        --files0-from=FILE
            Summarize usage of NUL-terminated paths specified in FILE.

        --json
            Output usage entries as JSON.

        --csv
            Output usage entries as CSV.

        --table
            Output usage entries as a formatted table.

        --pipe COMMAND
            Send usage output through COMMAND pipeline.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Output version information and exit.

    EXAMPLES
        du -h C:\Projects
            Estimate disk usage in C:\Projects using human-readable units.

        du -a --max-depth=2 .
            List all files and directories up to depth 2 in current directory.

        du -s -c C:\Users\Public C:\Temp
            Summarize disk usage for specified directories with a total.

        du --exclude=*.obj --json build
            Estimate usage in build directory excluding *.obj files in JSON.

    CrossShell for UNIX                                                        du(1)
)";
        }
    };
