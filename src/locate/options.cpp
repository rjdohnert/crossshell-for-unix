#include "options.hpp"
#include <iostream>

void CommandLineParser::printUsage() {
    std::cout << R"(locate(1)         CrossShell for UNIX Reference Manual                locate(1)

    NAME
        locate - search for files and directories using expression filters

    SYNOPSIS
        locate [OPTIONS] [PATH...] [EXPRESSION...]

    DESCRIPTION
        locate searches directory trees for files and directories matching
        specified criteria and expressions, executing actions such as printing
        or deletion. It supports predicate combinations, depth limits, and
        pattern filtering.

    OPTIONS
        -L, --follow
            Follow symbolic links during recursion.

        -P, --no-follow
            Do not follow symbolic links (default).

        --maxdepth N
            Limit search recursion depth to N levels.

        --mindepth N
            Do not apply tests or actions at levels less than N.

        --name PATTERN
            Match base filename against shell glob pattern.

        --path PATTERN
            Match full relative file path against pattern.

        --type TYPE
            Match file type: f (file), d (directory), l (symlink).

        --size N
            Match file size in bytes or scaled units.

        --mtime N
            Match file modification time in days.

        --mmin N
            Match file modification time in minutes.

        --empty
            Match empty files or empty directories.

        --print
            Print matched paths followed by newline (default).

        --print0
            Print matched paths followed by NUL character.

        --delete
            Delete matched files or directories.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        locate . --name "*.cpp"
            Find all C++ source files under current directory.

        locate C:\Projects --type f --size +10M
            Find files larger than 10 MB.

        locate . --maxdepth 2 --type d
            List directories up to 2 levels deep.

        locate src/ --name "*.tmp" --delete
            Find and delete temporary files in src/.

    CrossShell for UNIX                                                   locate(1)
)";
}

void CommandLineParser::printVersion() {
    std::cout << "locate v1.0.0\n";
}

std::string CommandLineParser::canonicalizeOption(const std::string& arg) {
    if (arg == "--name") return "-name";
    if (arg == "--iname") return "-iname";
    if (arg == "--path") return "-path";
    if (arg == "--ipath") return "-ipath";
    if (arg == "--type") return "-type";
    if (arg == "--size") return "-size";
    if (arg == "--mtime") return "-mtime";
    if (arg == "--mmin") return "-mmin";
    if (arg == "--empty") return "-empty";
    if (arg == "--maxdepth") return "-maxdepth";
    if (arg == "--mindepth") return "-mindepth";
    if (arg == "--print") return "-print";
    if (arg == "--print0") return "-print0";
    if (arg == "--delete") return "-delete";
    if (arg == "--not") return "-not";
    if (arg == "--and") return "-and";
    if (arg == "--or") return "-or";
    return arg;
}

LocateOptions CommandLineParser::parse(int argc, char* argv[]) {
    LocateOptions opts;
    int i = 1;
    bool stop_options = false;

    while (i < argc) {
        std::string arg = argv[i];

        if (stop_options) {
            break;
        }

        if (arg == "--") {
            stop_options = true;
            ++i;
            break;
        }

        if (arg == "--help" || arg == "-h") {
            opts.show_help = true;
            return opts;
        }

        if (arg == "--version" || arg == "-V") {
            opts.show_version = true;
            return opts;
        }

        if (arg == "-L" || arg == "--follow") {
            opts.follow_symlinks = true;
            i++;
        } else if (arg == "-P" || arg == "--no-follow") {
            opts.follow_symlinks = false;
            i++;
        } else {
            break;
        }
    }

    // Parse starting paths
    while (i < argc) {
        std::string arg = argv[i];
        if (arg == "!" || arg == "(" || arg == ")" || (!arg.empty() && arg[0] == '-')) {
            break; // Start of expression
        }
        opts.start_paths.push_back(arg);
        i++;
    }

    // Default path to current directory '.' if none specified
    if (opts.start_paths.empty()) {
        opts.start_paths.push_back(".");
    }

    // Remaining tokens are expression components
    while (i < argc) {
        opts.expr_tokens.push_back(canonicalizeOption(argv[i++]));
    }

    return opts;
}
