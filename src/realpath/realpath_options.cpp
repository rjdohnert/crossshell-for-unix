#include "realpath_options.hpp"

void RealpathOptions::printVersion() {
        std::cout << PROGRAM_NAME << " " << VERSION << "\n"
                  << "Copyright (C) 2026 Roberto J Dohnert\n";
    }

void RealpathOptions::printHelp() {
        std::cout << R"(realpath(1)                CrossShell for UNIX Reference Manual                realpath(1)

    NAME
        realpath - print the resolved absolute path

    SYNOPSIS
        realpath [OPTIONS] FILE...

    DESCRIPTION
        realpath resolves all symbolic links, junctions, and relative path
        references ('.' and '..') to print the canonical absolute path for
        each specified FILE.

    OPTIONS
        -e, --canonicalize-existing
            All components of the path must exist (default behavior).

        -m, --canonicalize-missing
            No components of the path need exist.

        -s, --strip, --no-symlinks
            Resolve '..' and '.' path components without expanding symbolic links.

        -q, --quiet
            Suppress error messages for paths that cannot be resolved.

        --relative-to=<dir>
            Print the resolved path relative to directory <dir>.

        --relative-base=<dir>
            Print the path relative to <dir> if within <dir>; otherwise print
            the canonical absolute path.

        -z, --zero
            End each output line with NUL (0 byte) instead of a newline.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        realpath ../file.txt
            Print the canonical absolute path for ../file.txt.

        realpath -m non_existent/path/file.txt
            Canonicalize path without requiring intermediate directories to exist.

        realpath --relative-to=/usr /usr/bin/tool
            Print path relative to /usr (output: bin/tool).

        realpath -z *.log
            Print resolved paths separated by NUL characters.

    CrossShell for UNIX                                                          realpath(1)
)";
    }

bool RealpathOptions::parse(int argc, char* argv[], RealpathOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];

            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printHelp();
                std::exit(0);
            } else if (arg == "--version" || arg == "-V") {
                printVersion();
                std::exit(0);
            } else if (arg == "-e" || arg == "--canonicalize-existing") {
                opts.mode = CanonicalMode::Existing;
            } else if (arg == "-m" || arg == "--canonicalize-missing") {
                opts.mode = CanonicalMode::Missing;
            } else if (arg == "-s" || arg == "--strip" || arg == "--no-symlinks") {
                opts.mode = CanonicalMode::NoSymlinks;
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
            } else if (arg == "-z" || arg == "--zero") {
                opts.zero = true;
            } else if (arg.rfind("--relative-to=", 0) == 0) {
                opts.relativeTo = fs::path(arg.substr(14));
            } else if (arg == "--relative-to") {
                if (i + 1 < argc) {
                    opts.relativeTo = fs::path(argv[++i]);
                } else {
                    std::cerr << PROGRAM_NAME << ": option '--relative-to' requires an argument\n";
                    return false;
                }
            } else if (arg.rfind("--relative-base=", 0) == 0) {
                opts.relativeBase = fs::path(arg.substr(16));
            } else if (arg == "--relative-base") {
                if (i + 1 < argc) {
                    opts.relativeBase = fs::path(argv[++i]);
                } else {
                    std::cerr << PROGRAM_NAME << ": option '--relative-base' requires an argument\n";
                    return false;
                }
            } else if (arg == "--") {
                for (++i; i < argc; ++i) {
                    opts.files.emplace_back(argv[i]);
                }
                break;
            } else if (arg.rfind("-", 0) == 0 && arg.length() > 1) {
                for (size_t c = 1; c < arg.length(); ++c) {
                    switch (arg[c]) {
                        case 'e': opts.mode = CanonicalMode::Existing; break;
                        case 'm': opts.mode = CanonicalMode::Missing; break;
                        case 's': opts.mode = CanonicalMode::NoSymlinks; break;
                        case 'q': opts.quiet = true; break;
                        case 'z': opts.zero = true; break;
                        default:
                            std::cerr << PROGRAM_NAME << ": unrecognized option '-" << arg[c] << "'\n"
                                      << "Try '" << PROGRAM_NAME << " --help' for more information.\n";
                            return false;
                    }
                }
            } else {
                opts.files.emplace_back(arg);
            }
        }

        if (opts.files.empty()) {
            std::cerr << PROGRAM_NAME << ": missing operand\n"
                      << "Try '" << PROGRAM_NAME << " --help' for more information.\n";
            return false;
        }

        return true;
    }
