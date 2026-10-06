#include "option_parser.hpp"
#include "rm_options.hpp"

void OptionParser::ShowHelp() {
        std::cout << R"(rm(1)                      CrossShell for UNIX Reference Manual                     rm(1)

    NAME
        rm - remove files or directories

    SYNOPSIS
        rm [OPTIONS] FILE...

    DESCRIPTION
        rm removes each specified FILE or directory from the filesystem. By
        default, it does not remove directories unless -r, -R, or -d is specified.
        If '-' or '--from-stdin' is provided (or when standard input is
        redirected without positional arguments), paths are read from standard
        input.

    OPTIONS
        -f, --force
            Ignore nonexistent files and arguments, never prompt. Automatically
            clears Windows Read-Only file attributes when deleting.

        -i
            Prompt for confirmation before every removal.

        -I
            Prompt once before removing more than three files or when removing
            recursively.

        --interactive[=when]
            Prompt according to <when>: 'never', 'once' (-I), or 'always' (-i).

        -r, -R, --recursive
            Remove directories and their contents recursively.

        -d, --dir
            Remove empty directories.

        -v, --verbose
            Display diagnostic messages detailing each removed file or directory.

        -n, --dry-run
            Simulate execution without modifying the filesystem.

        --preserve-root
            Do not remove drive roots (e.g., C:\ or /) [default].

        --no-preserve-root
            Do not treat drive roots specially.

        -0, --null
            Read input paths separated by NUL (0 byte) instead of whitespace.

        --from-stdin
            Read target paths from standard input.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        rm file1.txt file2.log
            Remove file1.txt and file2.log.

        rm -rf build C:\Temp\cache
            Force recursively delete build directory and cache.

        rm -rvn folder_to_delete
            Simulate recursive removal with verbose output.

        rm -d empty_dir
            Remove empty directory empty_dir.

    CrossShell for UNIX                                                          rm(1)
)";
    }

void OptionParser::ShowVersion() {
        std::cout << "rm v1.2.0)\n";
        std::cout << "Copyright (C) 2026, Roberto J Dohnert.\n";
    }

bool OptionParser::Parse(int argc, char* argv[], RmOptions& opts) const {
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                opts.showHelp = true;
                return true;
            } else if (arg == "--version") {
                opts.showVersion = true;
                return true;
            } else if (arg == "-f" || arg == "--force") {
                opts.force = true;
                opts.interactive = InteractiveMode::Never;
            } else if (arg == "-i") {
                opts.interactive = InteractiveMode::Always;
                opts.force = false;
            } else if (arg == "-I") {
                opts.interactive = InteractiveMode::Once;
            } else if (arg.rfind("--interactive", 0) == 0) {
                size_t eqPos = arg.find('=');
                if (eqPos == std::string_view::npos) {
                    opts.interactive = InteractiveMode::Always;
                } else {
                    std::string_view mode = arg.substr(eqPos + 1);
                    if (mode == "always") opts.interactive = InteractiveMode::Always;
                    else if (mode == "once") opts.interactive = InteractiveMode::Once;
                    else if (mode == "never") opts.interactive = InteractiveMode::Never;
                }
            } else if (arg == "-r" || arg == "-R" || arg == "--recursive") {
                opts.recursive = true;
            } else if (arg == "-d" || arg == "--dir") {
                opts.removeEmptyDirs = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-n" || arg == "--dry-run") {
                opts.dryRun = true;
            } else if (arg == "-0" || arg == "--null") {
                opts.nullDelimited = true;
            } else if (arg == "--from-stdin") {
                opts.readStdin = true;
            } else if (arg == "--preserve-root") {
                opts.preserveRoot = true;
            } else if (arg == "--no-preserve-root") {
                opts.preserveRoot = false;
            } else if (arg == "-") {
                opts.readStdin = true;
            } else if (arg.length() > 1 && arg[0] == '-' && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    switch (arg[j]) {
                        case 'f': opts.force = true; opts.interactive = InteractiveMode::Never; break;
                        case 'i': opts.interactive = InteractiveMode::Always; opts.force = false; break;
                        case 'I': opts.interactive = InteractiveMode::Once; break;
                        case 'r':
                        case 'R': opts.recursive = true; break;
                        case 'd': opts.removeEmptyDirs = true; break;
                        case 'v': opts.verbose = true; break;
                        case 'n': opts.dryRun = true; break;
                        case '0': opts.nullDelimited = true; break;
                        default:
                            std::cerr << "rm: invalid option -- '" << arg[j] << "'\n";
                            std::cerr << "Try 'rm --help' for more information.\n";
                            return false;
                    }
                }
            } else {
                positional.push_back(std::string(arg));
            }
        }

        if (positional.empty() && !opts.readStdin) {
            if (!_isatty(_fileno(stdin))) {
                opts.readStdin = true;
            }
        }

        opts.targets = std::move(positional);
        return true;
    }
