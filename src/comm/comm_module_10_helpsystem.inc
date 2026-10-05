    class HelpSystem {
    public:
        static void print_version() {
            std::cout << "comm version 2.9.0\n"
                      << "Copyright (C) 2026, Roberto J Dohnert\n";
        }

        static void print_help() {
            std::cout << R"(comm(1)                 CrossShell for UNIX Reference Manual                  comm(1)

    NAME
        comm - compare two sorted line streams

    SYNOPSIS
        comm [OPTIONS] FILE1 FILE2

    DESCRIPTION
        Merges two sorted inputs into columns for lines unique to FILE1, unique to
        FILE2, and common to both. One input may be '-', but both may not be.

    OPTIONS
        -1
            Suppress column 1 (lines unique to FILE1).

        -2
            Suppress column 2 (lines unique to FILE2).

        -3
            Suppress column 3 (lines that appear in both files).

        --check-order
            Check that the input is correctly sorted (default).

        --nocheck-order
            Disable sorted-order checking.

        --output-delimiter STRING
            Set the column delimiter (default is TAB).

        --total
            Append column counts and summary total at the end.

        -i, --ignore-case
            Compare lines case-insensitively.

        -z, --zero-terminated
            Use NUL line delimiters instead of newlines.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        comm file1.txt file2.txt
            Produce standard three-column comparison.

        comm -12 file1.txt file2.txt
            Print only lines common to both files.

        comm -23 file1.txt file2.txt
            Print lines unique to file1.

        comm --output-delimiter=, file1.txt file2.txt
            Use comma delimiter between columns.

    CrossShell for UNIX                                                    comm(1)
)";
        }
    };

    // =========================================================================
    // Main Application Coordinator
    // =========================================================================
