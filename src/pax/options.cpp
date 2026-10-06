#include "options.hpp"

void OptionParser::PrintHelp() {
    std::cout << R"(pax(1)                  CrossShell for UNIX Reference Manual                  pax(1)

    NAME
        pax - portable archive interchange

    SYNOPSIS
        pax [-v] [-f ARCHIVE] [-s REPL] [PATTERN...]
        pax -r [-k] [-u] [-v] [-f ARCHIVE] [-s REPL] [PATTERN...]
        pax -w [-v] [-f ARCHIVE] [-s REPL] FILE...
        pax -r -w [-k] [-u] [-v] [-s REPL] FILE... DIRECTORY

    DESCRIPTION
        pax reads, writes, and lists the members of an archive file and copies
        directory hierarchies. A variety of archive formats are supported,
        defaulting to POSIX ustar.

    OPTIONS
        -r, --read
            Read an archive from standard input or -f file and extract files.

        -w, --write
            Write files to standard output or -f file in archive format.

        -f, --file ARCHIVE
            Specify archive file name instead of standard input/output.

        -k, --keep
            Prevent overwriting existing files when extracting or copying.

        -u, --update
            Copy or extract only if the source file is newer than target.

        -v, --verbose
            List file names and metadata as they are processed.

        -s, --substitute REPL
            Modify file names using regular expression substitution /old/new/[g].

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    EXAMPLES
        pax -f archive.tar
            List contents of tar archive.

        pax -r -f archive.tar .
            Extract archive into current directory.

        pax -w -f backup.tar src/
            Create new archive backup.tar containing src/ directory.

        pax -r -w -v srcdir destdir
            Copy directory hierarchy from srcdir to destdir preserving structure.

    CrossShell for UNIX                                                    pax(1)
)";
}

void OptionParser::PrintVersion() {
    std::cout << "pax v1.0.0\n";
}

bool OptionParser::Parse(int argc, char* argv[], PaxOptions& opts, bool& exitEarly) const {
    exitEarly = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--") {
            for (int j = i + 1; j < argc; ++j) {
                opts.positionals.push_back(argv[j]);
            }
            break;
        } else if (arg == "--help" || arg == "-h") {
            PrintHelp();
            exitEarly = true;
            return true;
        } else if (arg == "--version" || arg == "-V") {
            PrintVersion();
            exitEarly = true;
            return true;
        } else if (arg == "-r" || arg == "--read") {
            opts.mode_read = true;
        } else if (arg == "-w" || arg == "--write") {
            opts.mode_write = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg == "-k" || arg == "--keep") {
            opts.keep = true;
        } else if (arg == "-u" || arg == "--update") {
            opts.update = true;
        } else if (arg == "-f" || arg == "--file") {
            if (i + 1 >= argc) {
                std::cerr << "pax: option '" << arg << "' requires a filename\n";
                return false;
            }
            opts.archive_file = argv[++i];
        } else if (arg == "-s" || arg == "--substitute") {
            if (i + 1 >= argc) {
                std::cerr << "pax: option '" << arg << "' requires an expression\n";
                return false;
            }
            if (!SubstitutionEngine::ParseSubstitution(argv[++i], opts.substitutions)) {
                return false;
            }
        } else if (arg.rfind("-", 0) == 0 && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                char c = arg[j];
                if (c == 'r') opts.mode_read = true;
                else if (c == 'w') opts.mode_write = true;
                else if (c == 'v') opts.verbose = true;
                else if (c == 'k') opts.keep = true;
                else if (c == 'u') opts.update = true;
                else {
                    std::cerr << "pax: invalid option '-" << c << "'\n";
                    return false;
                }
            }
        } else {
            opts.positionals.push_back(arg);
        }
    }
    return true;
}
