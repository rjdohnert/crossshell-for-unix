#include "xargs_options.hpp"

void XargsOptions::printUsage(const char* progName) {
        (void)progName;
        std::cout << R"(xargs(1)                  CrossShell for UNIX Reference Manual                xargs(1)

    NAME
        xargs - build and execute command lines from standard input

    SYNOPSIS
        xargs [OPTIONS] [COMMAND [INITIAL-ARGS]...]

    DESCRIPTION
        Build and execute command lines from standard input.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -0, --null
            Input items are terminated by a null character instead of by
            whitespace.

        -d, --delimiter CHARACTER
            Input items are terminated by the specified character.

        -n, --max-args MAX-ARGS
            Use at most MAX-ARGS arguments per command line.

        -L, --max-lines MAX-LINES
            Use at most MAX-LINES non-empty input lines per command line.

        -I REPLACE-STR
            Replace occurrences of REPLACE-STR in initial arguments with names
            read from standard input.

        -t, --verbose
            Print the command line on the standard error output before
            executing it.

        -r, --no-run-if-empty
            If the standard input does not contain any nonblanks, do not run
            the command.

        -h, --help, /?, -?
            Display this help and exit.

        -v, --version
            Output version information and exit.

    EXAMPLES
        xargs echo
            Read items from standard input and echo them as arguments.

        find . -name "*.txt" | xargs -n 1 type
            Display the contents of each file one at a time.

        dir /b | xargs -I {} cmd /c echo Processing {}
            Execute a command for each line replacing {} with the item.

    CrossShell for UNIX                                                   xargs(1)
)";
    }

void XargsOptions::printVersion() {
        std::cout << "xargs (CrossShell) 1.0\n";
    }

bool XargsOptions::parse(int argc, char* argv[], XargsOptions& opts) {
        int i = 1;
        for (; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h" || arg == "/?" || arg == "-?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version" || arg == "-v" || arg == "-V") {
                printVersion();
                std::exit(0);
            } else if (arg == "-0" || arg == "--null") {
                opts.mode = DelimitMode::NULL_CHAR;
            } else if (arg == "-t" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-r" || arg == "--no-run-if-empty") {
                opts.noRunIfEmpty = true;
            } else if ((arg == "-n" || arg == "--max-args") && i + 1 < argc) {
                opts.maxArgs = std::stoi(argv[++i]);
            } else if ((arg == "-L" || arg == "--max-lines") && i + 1 < argc) {
                opts.maxLines = std::stoi(argv[++i]);
                opts.mode = DelimitMode::NEWLINE;
            } else if ((arg == "-d" || arg == "--delimiter") && i + 1 < argc) {
                opts.delimiter = argv[++i][0];
                opts.mode = DelimitMode::DELIMITER;
            } else if (arg == "-I" && i + 1 < argc) {
                opts.replaceStr = argv[++i];
            } else if (arg == "--") {
                ++i;
                break;
            } else if (arg[0] == '-') {
                std::cerr << "xargs: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                break;
            }
        }

        for (; i < argc; ++i) {
            opts.command.push_back(argv[i]);
        }

        if (opts.command.empty()) {
            opts.command.push_back("echo");
        }

        return true;
    }
