#include "zip_help.hpp"

void print_usage(const char* prog_name) {
    std::cout << R"(zip(1)                  CrossShell for UNIX Reference Manual                   zip(1)

    NAME
        zip - package and compress (archive) files into ZIP format

    SYNOPSIS
        zip [OPTIONS] ARCHIVE [FILE...]

    DESCRIPTION
        zip is a compression and file packaging utility for Windows NTFS/ReFS
        and FAT systems.

    OPTIONS
        -r, --recurse-paths
            Travel the directory structure recursively.

        -j, --junk-paths
            Store just names of saved files (junk the paths).

        -q, --quiet
            Quiet mode; eliminate informational messages.

        -0..-9
            Compression level (0=store, 9=best compression, 6=default).

        -f, --freshen
            Freshen existing archive entries only.

        -x PATTERN
            Exclude files matching pattern.

        --json, --csv, --table
            Output archive creation summary as JSON, CSV, or table.

        --pipe COMMAND
            Send output into COMMAND.

        --help
            Display this reference manual.

    EXAMPLES
        zip -r backup.zip src/ docs/
            Recursively package src and docs into backup.zip.

    CrossShell for UNIX                                                    zip(1)
)";
}
