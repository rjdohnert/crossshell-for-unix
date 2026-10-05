class CsplitOptions {
public:
    std::string prefix{"xx"};
    int digits{2};
    bool quiet{false};
    bool keepFiles{false};
    std::string suffixFormat;
    std::string inputFile;
    std::vector<SplitPattern> patterns;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* progName = nullptr) {
        (void)progName;
        std::cout << R"(csplit(1)               CrossShell for UNIX Reference Manual           csplit(1)

    NAME
        csplit - split a file into sections determined by context lines

    SYNOPSIS
        csplit [OPTIONS] FILE PATTERN...

    DESCRIPTION
        Output pieces of FILE separated by PATTERN(s) to files 'xx00', 'xx01', ...,
        and output byte counts of each piece to standard output.

    OPTIONS
        -f, --prefix PREFIX
            Use PREFIX instead of 'xx'.

        -n, --digits DIGITS
            Use DIGITS digits instead of default 2.

        -s, -q, --silent, --quiet
            Do not print counts of output file sizes.

        -k, --keep-files
            Do not remove output files on errors.

        -b, --suffix-format FORMAT
            Use sprintf FORMAT for output file suffixes.

        --json, --csv, --table
            Output structured operation records in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        csplit file.txt /CHAPTER/ {*}
            Split file at each occurrence of CHAPTER.

        csplit -f section_ -n 3 server.log 100 200
            Split at line numbers 100 and 200 with 3-digit suffixes.

        csplit --json data.txt /SECTION/ {*}
            Split data and emit JSON status records.

    CrossShell for UNIX                                                    csplit(1)
)";
    }

    static void printVersion() {
        std::cout << "csplit 1.0\n";
    }

    static bool parsePattern(const std::string& arg, SplitPattern& pat) {
        if (arg.empty()) return false;

        if (arg[0] == '/' || arg[0] == '%') {
            char delim = arg[0];
            pat.type = (delim == '/') ? PatternType::RegexMatch : PatternType::RegexSkip;
            size_t endDelim = arg.find(delim, 1);
            if (endDelim == std::string::npos) return false;

            pat.regexStr = arg.substr(1, endDelim - 1);
            if (endDelim + 1 < arg.size()) {
                pat.offset = std::stoll(arg.substr(endDelim + 1));
            }
            return true;
        } else if (arg[0] == '{') {
            if (arg.back() != '}') return false;
            std::string rep = arg.substr(1, arg.size() - 2);
            if (rep == "*") pat.repeat = -1;
            else pat.repeat = std::stoi(rep);
            return true;
        } else if (std::isdigit(static_cast<unsigned char>(arg[0]))) {
            pat.type = PatternType::LineNum;
            pat.lineNum = std::stoll(arg);
            return true;
        }
        return false;
    }

    static bool parse(int argc, char* argv[], CsplitOptions& opts) {
        int i = 1;
        for (; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-k" || arg == "--keep-files") {
                opts.keepFiles = true;
            } else if (arg == "-s" || arg == "--quiet" || arg == "--silent") {
                opts.quiet = true;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if ((arg == "-f" || arg == "--prefix") && i + 1 < argc) {
                opts.prefix = argv[++i];
            } else if ((arg == "-n" || arg == "--digits") && i + 1 < argc) {
                opts.digits = std::stoi(argv[++i]);
            } else if ((arg == "-b" || arg == "--suffix-format") && i + 1 < argc) {
                opts.suffixFormat = argv[++i];
            } else if (arg == "--") {
                ++i;
                break;
            } else if (arg[0] == '-') {
                std::cerr << "csplit: unknown option '" << arg << "'\n";
                return false;
            } else {
                break;
            }
        }

        if (i >= argc) {
            std::cerr << "csplit: missing file operand\n";
            return false;
        }
        opts.inputFile = argv[i++];

        for (; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg[0] == '{' && !opts.patterns.empty()) {
                SplitPattern repeatPat;
                if (parsePattern(arg, repeatPat)) {
                    opts.patterns.back().repeat = repeatPat.repeat;
                }
            } else {
                SplitPattern pat;
                if (parsePattern(arg, pat)) {
                    opts.patterns.push_back(pat);
                } else {
                    std::cerr << "csplit: invalid pattern '" << arg << "'\n";
                    return false;
                }
            }
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

