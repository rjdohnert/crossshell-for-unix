#include "options.hpp"

void HelpFormatter::printVersion() {
    std::cout << "mv version 1.0.0\n";
}

void HelpFormatter::printHelp() {
    std::cout << R"(mv(1)                   CrossShell for UNIX Reference Manual                      mv(1)

    NAME
        mv - move or rename files and directory hierarchies

    SYNOPSIS
        mv [OPTIONS] SOURCE... DESTINATION

    DESCRIPTION
        The mv command moves or renames files and directories. If the
        destination is an existing directory, the source files are moved
        into that directory.

        If source and destination reside on the same filesystem volume,
        mv executes an instantaneous atomic rename. When moving across
        different volumes or file system boundaries (e.g., C: to D:), mv
        falls back to a chunked stream transfer with dynamic progress
        reporting, reproduces all timestamps and security descriptors, and
        unlinks the source.

    OPTIONS
        -e
            Preserve file attributes, Windows Security Descriptors, and
            Extended Attributes during cross-filesystem moves.

        -f, --force
            Overwrite target files without prompting, even if permissions
            would otherwise prevent writing. Overrides prior -i options.

        -i, --interactive
            Prompt for confirmation before overwriting an existing target
            file. Overrides prior -f options.

        -v, --verbose
            Display detailed operational notices upon file moves.

        --json
            Output structured JSON summary of operations.

        --no-progress
            Disable the interactive progress bar during cross-device
            transfers.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version information.

    EXAMPLES
        mv update.tar update_old.tar
            Rename a file in place.

        mv file1.log file2.log C:\Archive\
            Move multiple files into a target directory.

        mv -i database.mdf D:\LiveMount\
            Prompt before overwriting destination files across drives.

        mv -fe SourceTree E:\BackupTree\
            Move a directory tree preserving extended attributes and forcing overwrite.

    CrossShell for UNIX                                                    mv(1)
)";
}

bool ArgumentParser::parse(int argc, char* argv[], MoveOptions& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h" || arg == "/?") {
            options.showHelp = true;
            return true;
        }
        if (arg == "--version" || arg == "-V") {
            options.showVersion = true;
            return true;
        }
        if (arg == "--no-progress") {
            options.showProgress = false;
            continue;
        }
        if (arg == "--verbose" || arg == "-v") {
            options.verbose = true;
            continue;
        }
        if (arg == "--json") {
            options.outputJson = true;
            options.showProgress = false;
            continue;
        }

        if (arg.rfind("--", 0) == 0) {
            std::cerr << "mv: unrecognized option '" << arg << "'\nTry 'mv --help' for manual.\n";
            return false;
        }

        if (arg.length() > 1 && arg[0] == '-') {
            for (size_t c = 1; c < arg.length(); ++c) {
                switch (arg[c]) {
                    case 'f':
                        options.force = true;
                        options.interactive = false;
                        break;
                    case 'i':
                        options.interactive = true;
                        options.force = false;
                        break;
                    case 'e':
                        options.hpuxExtentAcl = true;
                        break;
                    case 'v':
                        options.verbose = true;
                        break;
                    default:
                        std::cerr << "mv: illegal option -- " << arg[c] << "\n";
                        std::cerr << "usage: mv [-f | -i] [-e] file ... target\n";
                        return false;
                }
            }
        } else {
            options.sources.push_back(fs::u8path(arg));
        }
    }

    if (options.sources.size() < 2) {
        std::cerr << "mv: missing destination file operand after '" 
                  << (!options.sources.empty() ? options.sources[0].string() : "") << "'\n"
                  << "Try 'mv --help' for more information.\n";
        return false;
    }

    options.destination = options.sources.back();
    options.sources.pop_back();

    return true;
}
