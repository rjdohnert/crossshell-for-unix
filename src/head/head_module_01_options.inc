// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class HeadOptions {
public:
    long long lineCount{10};
    long long byteCount{0};
    bool useBytes{false};
    bool quiet{false};
    bool verbose{false};
    int outputFormat{0}; // 0: raw, 1: JSON, 2: CSV, 3: Table
    std::string pipeCommand;
    std::vector<std::string> files;

    static void printUsage(const char* /*progName*/) {
        std::cout << R"HELP(head(1)                 CrossShell for UNIX Reference Manual                  head(1)

    NAME
        head - Output the first part of files or standard input.

    SYNOPSIS
        head [OPTIONS] [FILE...]

    DESCRIPTION
        Prints the first 10 lines of each FILE to standard output. A line or
        byte limit may be selected. With multiple files, each result is
        preceded by a file-name header unless quiet mode is enabled. When no
        FILE is supplied, or FILE is -, input is read from standard input.

    OPTIONS
        -n, --lines NUM
            Print the first NUM lines instead of 10.

        -NUM
            Print the first NUM lines using the traditional shorthand.

        -c, --bytes NUM
            Print the first NUM bytes of each file.

        -q, --quiet
            Suppress file-name headers.

        -v, --verbose
            Always print file-name headers.

        --json
            Format selected content as JSON.

        --csv
            Format selected content as CSV.

        --table
            Format selected content as a table.

        --pipe COMMAND
            Send output through COMMAND.

        --
            End option processing; treat remaining arguments as file names.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        head document.txt
            Display the first 10 lines of document.txt.

        head -n 20 document.txt
            Display the first 20 lines of document.txt.

        head -c 512 binary.dat
            Display the first 512 bytes of binary.dat.

        head -n 5 file1.txt file2.txt
            Display the first 5 lines of each file with file headers.

        type document.txt | head -5
            Display the first five lines received from standard input.

        head --json -n 5 document.txt
            Format the selected content as JSON.

    CrossShell for UNIX                                                        head(1)
)HELP";
    }

    static void printVersion() {
        std::cout << "head 1.0\n";
    }

    static bool parseInt(const std::string& str, long long& val) {
        if (str.empty()) return false;
        std::stringstream ss(str);
        ss >> val;
        return !ss.fail() && ss.eof() && val >= 0;
    }

    static bool parse(int argc, char* argv[], HeadOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--") {
                for (int j = i + 1; j < argc; ++j) {
                    opts.files.push_back(argv[j]);
                }
                break;
            } else if (arg == "-") {
                opts.files.push_back("-");
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg == "--help" || arg == "-h" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version" || arg == "-V") {
                printVersion();
                std::exit(0);
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
                opts.verbose = false;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
                opts.quiet = false;
            } else if (arg.rfind("--lines=", 0) == 0) {
                std::string valStr = arg.substr(8);
                long long val = 0;
                if (!parseInt(valStr, val)) {
                    std::cerr << "head: illegal line count -- " << valStr << "\n";
                    return false;
                }
                opts.lineCount = val;
                opts.useBytes = false;
            } else if (arg.rfind("--bytes=", 0) == 0) {
                std::string valStr = arg.substr(8);
                long long val = 0;
                if (!parseInt(valStr, val)) {
                    std::cerr << "head: illegal byte count -- " << valStr << "\n";
                    return false;
                }
                opts.byteCount = val;
                opts.useBytes = true;
            } else if (arg == "-n" || arg == "--lines") {
                if (i + 1 >= argc) {
                    std::cerr << "head: option requires an argument -- n\n";
                    printUsage(argv[0]);
                    return false;
                }
                long long val = 0;
                if (!parseInt(argv[++i], val)) {
                    std::cerr << "head: illegal line count -- " << argv[i] << "\n";
                    return false;
                }
                opts.lineCount = val;
                opts.useBytes = false;
            } else if (arg == "-c" || arg == "--bytes") {
                if (i + 1 >= argc) {
                    std::cerr << "head: option requires an argument -- c\n";
                    printUsage(argv[0]);
                    return false;
                }
                long long val = 0;
                if (!parseInt(argv[++i], val)) {
                    std::cerr << "head: illegal byte count -- " << argv[i] << "\n";
                    return false;
                }
                opts.byteCount = val;
                opts.useBytes = true;
            } else if (arg[0] == '-' && arg.size() > 1) {
                if (std::isdigit(static_cast<unsigned char>(arg[1]))) {
                    long long val = 0;
                    if (parseInt(arg.substr(1), val)) {
                        opts.lineCount = val;
                        opts.useBytes = false;
                    } else {
                        std::cerr << "head: illegal line count -- " << arg.substr(1) << "\n";
                        return false;
                    }
                } else {
                    for (size_t j = 1; j < arg.size(); ++j) {
                        char option = arg[j];
                        if (option == 'h') {
                            printUsage(argv[0]);
                            std::exit(0);
                        } else if (option == 'q') {
                            opts.quiet = true;
                            opts.verbose = false;
                        } else if (option == 'v') {
                            opts.verbose = true;
                            opts.quiet = false;
                        } else if (option == 'n' || option == 'c') {
                            std::string valStr;
                            if (j + 1 < arg.size()) {
                                valStr = arg.substr(j + 1);
                                j = arg.size();
                            } else if (i + 1 < argc) {
                                valStr = argv[++i];
                            } else {
                                std::cerr << "head: option requires an argument -- " << option << "\n";
                                printUsage(argv[0]);
                                return false;
                            }
                            long long val = 0;
                            if (!parseInt(valStr, val)) {
                                std::cerr << "head: illegal " << (option == 'n' ? "line" : "byte") << " count -- " << valStr << "\n";
                                return false;
                            }
                            if (option == 'n') {
                                opts.lineCount = val;
                                opts.useBytes = false;
                            } else {
                                opts.byteCount = val;
                                opts.useBytes = true;
                            }
                        } else {
                            std::cerr << "head: unknown option -- " << option << "\n";
                            printUsage(argv[0]);
                            return false;
                        }
                    }
                }
            } else {
                opts.files.push_back(arg);
            }
        }

        if (opts.files.empty()) {
            opts.files.push_back("-");
        }

        return true;
    }
};

