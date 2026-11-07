/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "du_help_system.hpp"

#include <iostream>

namespace du {

void PrintVersion() {
    std::cout << "du version 5.1.0\n"
              << "Copyright (C) 2026, Roberto J Dohnert.\n";
}

void PrintHelp() {
    std::cout << R"(du(1)                     CrossShell for UNIX Reference Manual                     du(1)

    NAME
        du - estimate file and directory space usage

    SYNOPSIS
        du [OPTIONS] [FILE...]

    DESCRIPTION
        Reports file and directory usage in allocated or apparent bytes.
        With no FILE specified, the current directory is scanned.

    OPTIONS
        -a, --all
            Include individual files as well as directory totals.
        -s, --summarize
            Display only a summary for each operand.
        -c, --total
            Display a cumulative grand total.
        -b, --bytes
            Report apparent sizes in bytes.
        --apparent-size
            Use apparent sizes in the selected display unit.
        -0, --null
            Terminate human-readable output entries with NUL.
        -L, --dereference
            Follow symbolic links and Windows reparse points. Directory
            cycles and duplicate file identities are detected.
        -d, --max-depth=N
            Display directory totals only through depth N.
        -k
            Display 1024-byte blocks (default).
        -m
            Display 1048576-byte blocks.
        -H, --human-readable
            Display human-readable sizes (for example 1K, 234M, 2G).
        --exclude=PATTERN
            Exclude files and directories whose names match PATTERN.
        --files0-from=FILE
            Read NUL-terminated paths from FILE; use - for standard input.
        --json, --csv, --table
            Emit JSON, CSV, or tab-separated output.
        --pipe COMMAND
            Send output to COMMAND's standard input.
        -h, --help
            Display this reference manual and exit.
        -v, --version
            Display version information and exit.

    EXAMPLES
        du -H C:\Projects
        du -a --max-depth=2 .
        du -s -c C:\Users\Public C:\Temp
        du --exclude=*.obj --json build

    CrossShell for UNIX                                                        du(1)
)";
}

} // namespace du
