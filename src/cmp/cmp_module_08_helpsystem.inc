    class HelpSystem {
    public:
        static void print_version() {
            std::cout << "cmp version 2.6.0\n"
                      << "Copyright (C) 2026, Roberto J Dohnert\n";
        }

        static void print_help() {
            std::cout << R"(cmp(1)                  CrossShell for UNIX Reference Manual                  cmp(1)

    NAME
        cmp - compare two byte streams

    SYNOPSIS
        cmp [OPTIONS] FILE1 [FILE2 [SKIP1 [SKIP2]]]

    DESCRIPTION
        Compares byte streams byte by byte, optionally skipping initial bytes and
        limiting the comparison length. Either file may be '-' for standard input.

    OPTIONS
        -b, --print-bytes
            Print differing byte values.

        -i, --ignore-initial VALUE
            Skip bytes; specify either SKIP or S1:S2.

        -l, --verbose
            Print byte numbers and differing byte values.

        -n, --bytes LIMIT
            Compare at most LIMIT bytes.

        -s, -q, --silent, --quiet
            Suppress normal comparison output.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        cmp file1.bin file2.bin
            Compare two binary files byte by byte.

        cmp -l file1.bin file2.bin
            Output offsets and values of differing bytes.

        cmp -i 1k:2k -n 512 file1.bin file2.bin
            Skip initial bytes and compare first 512 bytes.

    CrossShell for UNIX                                                    cmp(1)
)";
        }
    };

    // =========================================================================
    // Main Application Orchestrator
    // =========================================================================
