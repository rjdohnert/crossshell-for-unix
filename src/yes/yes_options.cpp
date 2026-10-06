#include "yes_options.hpp"

void YesOptions::printHelp(const char* progName) {
        (void)progName;
        std::cout << R"(yes(1)                    CrossShell for UNIX Reference Manual                  yes(1)

    NAME
        yes - output a string repeatedly until killed

    SYNOPSIS
        yes [OPTIONS] [STRING...]

    DESCRIPTION
        Repeatedly output a line with all specified STRING(s), or 'y'.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        --
            Explicitly treat all subsequent arguments as input strings, even
            if they start with '-'.

        -h, --help, /?, -?
            Display this help and exit.

        -v, --version
            Output version information and exit.

    EXAMPLES
        yes
            Repeatedly output 'y'.

        yes confirmation
            Repeatedly output 'confirmation'.

        yes -- -h
            Repeatedly output '-h' without triggering help.

    CrossShell for UNIX                                                    yes(1)
)";
    }

void YesOptions::printVersion() {
        std::cout << "yes (CrossShell) " << VERSION << "\n"
                  << "Copyright (C) 2026 Roberto J Dohnert\n";
    }

bool YesOptions::parse(int argc, char* argv[], YesOptions& opts) {
        if (argc > 1) {
            std::string firstArg = argv[1];
            if (firstArg == "-h" || firstArg == "--help" || firstArg == "/?" || firstArg == "-?") {
                printHelp(argv[0]);
                std::exit(0);
            }
            if (firstArg == "-v" || firstArg == "--version") {
                printVersion();
                std::exit(0);
            }

            int startIndex = 1;
            if (firstArg == "--") {
                startIndex = 2;
            }

            std::string lineStr;
            for (int i = startIndex; i < argc; ++i) {
                if (i > startIndex) lineStr += " ";
                lineStr += argv[i];
            }
            if (!lineStr.empty()) {
                opts.line = lineStr;
            }
        }
        return true;
    }
