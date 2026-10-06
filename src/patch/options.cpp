#include "options.hpp"

void OptionParser::PrintHelp() {
    std::cout << R"(patch(1)                CrossShell for UNIX Reference Manual                  patch(1)

    NAME
        patch - apply a diff file to an original

    SYNOPSIS
        patch [OPTIONS] [ORIGFILE [PATCHFILE]]

    DESCRIPTION
        patch takes a patch file containing a difference listing produced by
        diff and applies those differences to one or more original files,
        generating patched versions.

    OPTIONS
        -p NUM, --strip NUM
            Strip NUM leading path components from file names found in diff.

        -i FILE, --input FILE
            Read patch from FILE instead of standard input.

        -o FILE, --output FILE
            Write output to FILE instead of modifying files in place.

        -R, --reverse
            Assume that the patch was created with the old and new files
            reversed; swap old and new changes.

        -b, --backup
            Make backup copies of files before modifying them.

        -z SUFFIX, --suffix SUFFIX
            Use SUFFIX instead of .orig as the backup file extension.

        -E, --remove-empty-files
            Remove output files that become empty after patching.

        -d DIR, --directory DIR
            Change working directory to DIR before applying patches.

        --dry-run
            Test applying patch without modifying files on disk.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    EXAMPLES
        patch -p1 -i fix.patch
            Apply unified diff stripping 1 leading path segment.

        patch -p0 -R -b -i fix.patch
            Revert patch and create backup files with .orig suffix.

        patch -p1 --dry-run -i fix.patch
            Check patch application without writing changes.

    CrossShell for UNIX                                                    patch(1)
)";
}

void OptionParser::PrintVersion() {
    std::cout << "patch v1.0.0\n";
}

bool OptionParser::Parse(int argc, char* argv[], PatchOptions& opts, bool& exitEarly) const {
    exitEarly = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                opts.positionalArgs.push_back(argv[j]);
            }
            break;
        } else if (arg == "-p" || arg == "--strip") {
            if (++i < argc) {
                try {
                    opts.stripCount = std::stoi(argv[i]);
                } catch (...) {
                    std::cerr << "patch: invalid strip count '" << argv[i] << "'\n";
                    return false;
                }
            }
        } else if (arg.rfind("-p", 0) == 0 && arg.size() > 2) {
            try {
                opts.stripCount = std::stoi(arg.substr(2));
            } catch (...) {
                std::cerr << "patch: invalid strip count '" << arg.substr(2) << "'\n";
                return false;
            }
        } else if (arg == "-i" || arg == "--input") {
            if (++i < argc) opts.patchFile = argv[i];
        } else if (arg == "-o" || arg == "--output") {
            if (++i < argc) opts.outputFile = argv[i];
        } else if (arg == "-b" || arg == "--backup") {
            opts.makeBackup = true;
        } else if (arg == "-z" || arg == "--suffix") {
            if (++i < argc) { opts.backupSuffix = argv[i]; opts.makeBackup = true; }
        } else if (arg == "-R" || arg == "--reverse") {
            opts.reverse = true;
        } else if (arg == "-E" || arg == "--remove-empty-files") {
            opts.removeEmpty = true;
        } else if (arg == "--dry-run") {
            opts.dryRun = true;
        } else if (arg == "-d" || arg == "--directory") {
            if (++i < argc) opts.changeDir = argv[i];
        } else if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp();
            exitEarly = true;
            return true;
        } else if (arg == "--version" || arg == "-V") {
            PrintVersion();
            exitEarly = true;
            return true;
        } else if (arg[0] != '-') {
            opts.positionalArgs.push_back(arg);
        } else {
            std::cerr << "patch: unknown option '" << arg << "'\n";
            std::cerr << "Try 'patch --help' for options.\n";
            return false;
        }
    }
    return true;
}
