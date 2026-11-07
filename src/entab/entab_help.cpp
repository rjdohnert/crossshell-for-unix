/*
 * BSD 3-Clause License
 * Copyright (c) 2026, Roberto J Dohnert
 */

#include "entab_help.hpp"

#include <iostream>

void PrintEntabVersion() {
    std::cout << "entab version 1.0.0\n"
              << "Copyright (C) 2026 Roberto J Dohnert\n";
}

void PrintEntabHelp(const char* programName) {
    std::cout << R"(entab(1)                  CrossShell for UNIX Reference Manual                  entab(1)

NAME
     entab - replace groups of spaces with tabs

SYNOPSIS
     )" << (programName ? programName : "entab") << R"( [OPTION]... [FILE]...

DESCRIPTION
     Convert eligible spaces and tabs in each FILE, writing to standard output.
     With no FILE, or when FILE is -, read standard input.

OPTIONS
     -a, --all
          Convert all eligible whitespace runs, not only initial whitespace.

     --first-only
          Convert only leading whitespace. This takes precedence over -a.

     -t N, --tabs=N
          Set regularly spaced tab stops N columns apart. Enables -a unless
          --first-only is specified.

     -t LIST, --tabs=LIST
          Set explicit, strictly increasing tab stops, for example
          --tabs=4,8,12 or --tabs="4 8 12". Enables -a unless --first-only
          is specified.

     -tLIST
          Attached short-option form for a tab specification.

     -N
          Shorthand for -t N (for example -4).

     --json, --csv, --table
          Emit converted text as escaped JSON, CSV, or a one-column table.
          Select only one format.

     --pipe COMMAND
          Send output to COMMAND's standard input and report command failures.

     -h, --help
          Display this reference manual and exit.

     -v, --version
          Display version information and exit.

     --
          Treat following arguments as file names, including names beginning
          with '-'.

EXAMPLES
     entab file.txt
     entab -4 data.txt
     entab --first-only -t 4 source.txt
     entab --tabs=4,8,12 --json report.txt
     type source.txt | entab

CrossShell for UNIX                                                        entab(1)
)";
}
