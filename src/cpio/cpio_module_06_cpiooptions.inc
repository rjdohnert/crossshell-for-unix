class CpioOptions {
public:
    CpioMode mode = CpioMode::None;
    bool verbose = false;
    bool make_dirs = false;
    bool null_delim = false;
    std::string archive_file;
    std::string pass_dir;
    OutputFormat outputFormat = OutputFormat::Default;
    std::string pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { outputFormat = OutputFormat::Json; continue; }
            if (arg == "--csv") { outputFormat = OutputFormat::Csv; continue; }
            if (arg == "--table") { outputFormat = OutputFormat::Table; continue; }
            if (arg == "--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }
            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    if (mode == CpioMode::Pass && pass_dir.empty()) {
                        pass_dir = argv[j];
                    } else {
                        std::cerr << "cpio: unexpected argument: " << argv[j] << "\n";
                        return false;
                    }
                }
                break;
            }
            if (arg == "--help" || arg == "-h" || arg == "/?") {
                showHelp = true;
                return true;
            }
            if (arg == "--version") {
                showVersion = true;
                return true;
            }
            if (arg.rfind("-", 0) == 0 && arg.length() > 1 && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    char c = arg[j];
                    switch (c) {
                        case 'o': mode = CpioMode::Create; break;
                        case 'i': if (mode != CpioMode::List) mode = CpioMode::Extract; break;
                        case 'p': mode = CpioMode::Pass; break;
                        case 't': mode = CpioMode::List; break;
                        case 'v': verbose = true; break;
                        case 'd': make_dirs = true; break;
                        case '0': null_delim = true; break;
                        case 'F':
                            if (j + 1 < arg.length()) {
                                archive_file = arg.substr(j + 1);
                                j = arg.length();
                            } else if (i + 1 < argc) {
                                archive_file = argv[++i];
                            } else {
                                std::cerr << "cpio: option requires an argument -- 'F'\n";
                                return false;
                            }
                            break;
                        default:
                            std::cerr << "cpio: unknown option -- '" << c << "'\n";
                            return false;
                    }
                }
            } else if (arg == "--create") mode = CpioMode::Create;
            else if (arg == "--extract") { if (mode != CpioMode::List) mode = CpioMode::Extract; }
            else if (arg == "--pass-through") mode = CpioMode::Pass;
            else if (arg == "--list") mode = CpioMode::List;
            else if (arg == "--verbose") verbose = true;
            else if (arg == "--make-directories") make_dirs = true;
            else if (arg == "--null") null_delim = true;
            else if (arg == "--file") {
                if (i + 1 < argc) archive_file = argv[++i];
                else { std::cerr << "cpio: option requires an argument -- 'file'\n"; return false; }
            } else if (arg.rfind("--file=", 0) == 0) {
                archive_file = arg.substr(7);
            } else if (mode == CpioMode::Pass && pass_dir.empty()) {
                pass_dir = arg;
            } else {
                std::cerr << "cpio: unexpected argument: " << arg << "\n";
                return false;
            }
        }
        return true;
    }

    void PrintHelp(const char* prog_name) const {
        std::cout << R"(cpio(1)                 CrossShell for UNIX Reference Manual                  cpio(1)

    NAME
        cpio - copy files to and from archives

    SYNOPSIS
        cpio -o [OPTIONS] < name-list > archive
        cpio -i [OPTIONS] [PATTERNS...] < archive
        cpio -p [OPTIONS] DEST-DIRECTORY < name-list

    DESCRIPTION
        cpio copies files into or out of a cpio or tar archive. It supports
        create mode (-o), extract mode (-i), and pass-through mode (-p).

    OPTIONS
        -o, --create
            Run in copy-out mode (create archive).

        -i, --extract
            Run in copy-in mode (extract from archive).

        -p, --pass-through
            Run in copy-pass mode (copy directory tree to target directory).

        -v, --verbose
            List files processed.

        -t, --list
            Print table of contents of the input.

        --json, --csv, --table
            Output archive entry manifests in structured format.

        --pipe COMMAND
            Stream results into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        find . -depth -print0 | cpio -ov > tree.cpio
            Archive current directory hierarchy into tree.cpio.

        cpio -ivt < tree.cpio
            List contents of tree.cpio archive.

    CrossShell for UNIX                                                   cpio(1)
)";
    }

    void PrintVersion() const {
        std::cout << "cpio 1.0\n";
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

