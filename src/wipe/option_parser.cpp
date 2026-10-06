#include "option_parser.hpp"
#include "wipe_options.hpp"

void OptionParser::PrintUsage(const char* prog_name) {
        (void)prog_name;
        std::cout << R"(wipe(1)                   CrossShell for UNIX Reference Manual                 wipe(1)

    NAME
        wipe - securely erase files and directory trees by overwriting data

    SYNOPSIS
        wipe [OPTIONS] [FILE/DIR...]

    DESCRIPTION
        Securely erase files and directory trees by overwriting data.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -f, --force
            Force deletion without prompt and override read-only.

        -r, -R, --recursive
            Remove directories and their contents recursively.

        -v, --verbose
            Verbose mode, output detailed wipe progress.

        -i, --interactive
            Prompt before wiping each file.

        -q, --quick
            Quick mode (2 passes: random + zeroes).

        -d, --dod
            DoD 5220.22-M mode (3 passes: zeroes, ones, random).

        -p, --passes N
            Set custom number of overwrite passes (default: 4).

        -h, --help, /?, -?
            Display this help and exit.

        --version
            Output version information and exit.

    EXAMPLES
        wipe file.txt
            Securely wipe a single file.

        wipe -r -f secret_dir
            Recursively wipe directory without confirmation prompts.

        wipe -q large_data.bin
            Quick wipe using 2 overwrite passes.

    CrossShell for UNIX                                                    wipe(1)
)";
    }

bool OptionParser::Parse(int argc, char* argv[], WipeOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?" || arg == "-?") {
                PrintUsage(argv[0]);
                exitEarly = true;
                return true;
            } else if (arg == "--version") {
                std::cout << "wipe (CrossShell) 1.0\n";
                exitEarly = true;
                return true;
            } else if (arg == "-f" || arg == "--force") {
                opts.force = true;
            } else if (arg == "-r" || arg == "-R" || arg == "--recursive") {
                opts.recursive = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-i" || arg == "--interactive") {
                opts.interactive = true;
            } else if (arg == "-q" || arg == "--quick") {
                opts.quick_mode = true;
            } else if (arg == "-d" || arg == "--dod") {
                opts.dod_mode = true;
            } else if (arg == "-p" || arg == "--passes") {
                if (i + 1 < argc) {
                    char* end = nullptr;
                    long parsed = std::strtol(argv[++i], &end, 10);
                    if (end == nullptr || *end != '\0' || parsed < 1 || parsed > 1000) {
                        std::cerr << "wipe: invalid number of passes: '" << argv[i] << "'\n";
                        return false;
                    }
                    opts.passes = static_cast<int>(parsed);
                } else {
                    std::cerr << "wipe: option '-p' requires an argument\n";
                    return false;
                }
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "wipe: invalid option '" << arg << "'\n";
                return false;
            } else {
                opts.targets.push_back(arg);
            }
        }

        if (opts.targets.empty()) {
            std::cerr << "wipe: missing file or directory operand\n";
            std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
            return false;
        }

        return true;
    }
