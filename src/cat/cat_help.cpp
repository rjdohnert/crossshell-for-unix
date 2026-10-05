#include "cat_options.hpp"

#include <iostream>

void HelpFormatter::printHelp() {
    std::cout << R"(cat(1)                  CrossShell for UNIX Reference Manual                  cat(1)

    NAME
        cat - concatenate and display files

    SYNOPSIS
        cat [OPTIONS] [FILE...]

    DESCRIPTION
        cat reads each FILE in sequence and writes it to standard output.
        If FILE is omitted or specified as '-', cat reads from standard input.

    OPTIONS
        -b
            Number non-empty output lines, overriding -n.

        -e
            Display a '$' at end of each line (implies -v).

        -n
            Number all output lines starting at 1.

        -r
            Squeeze multiple adjacent empty lines into a single blank line.

        -s
            Suppress error messages about nonexistent or unreadable files.

        -t
            Display TAB characters as '^I' and form feeds as '^L' (implies -v).

        -u
            Disable output buffering for character-by-character processing.

        -v
            Display non-printing characters visibly using caret notation.

        --json
            Emit structured output records in JSON format.

        -h, --help
            Display this reference manual.

    EXAMPLES
        cat file1.txt file2.txt
            Concatenate and print files to standard output.

        cat -n source.cpp
            Display file with numbered lines.

        cat -ben main.c
            Display line numbers and ends on non-empty lines.

    CrossShell for UNIX                                                    cat(1)
)";
}
