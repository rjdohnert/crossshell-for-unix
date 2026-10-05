class FdOptions {
public:
    std::string pattern;
    std::vector<std::string> searchPaths;
    bool includeHidden = false;
    bool includeIgnored = false;
    CaseMode caseMode = CaseMode::Smart;
    bool fixedStrings = false;
    bool absolutePaths = false;
    bool print0 = false;
    bool colorOutput = true;
    bool colorOutputExplicit = false;
    int maxDepth = -1;
    int minDepth = -1;
    std::vector<std::string> extensions;
    std::vector<char> fileTypes;
    std::string execCommand;
    OutputFormat outputFormat = OutputFormat::Default;
    std::string pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "--json") {
                outputFormat = OutputFormat::Json;
            } else if (arg == "--csv") {
                outputFormat = OutputFormat::Csv;
            } else if (arg == "--table") {
                outputFormat = OutputFormat::Table;
            } else if (arg == "--pipe" && i + 1 < argc) {
                pipeCommand = argv[++i];
            } else if (arg == "-H" || arg == "--hidden") {
                includeHidden = true;
            } else if (arg == "-I" || arg == "--no-ignore") {
                includeIgnored = true;
            } else if (arg == "-s" || arg == "--case-sensitive") {
                caseMode = CaseMode::Sensitive;
            } else if (arg == "-i" || arg == "--ignore-case") {
                caseMode = CaseMode::Insensitive;
            } else if (arg == "-F" || arg == "--fixed-strings") {
                fixedStrings = true;
            } else if (arg == "-a" || arg == "--absolute") {
                absolutePaths = true;
            } else if (arg == "-0" || arg == "--print0") {
                print0 = true;
            } else if (arg == "-d" || arg == "--max-depth") {
                if (i + 1 < argc) maxDepth = std::atoi(argv[++i]);
            } else if (arg.rfind("--max-depth=", 0) == 0) {
                maxDepth = std::atoi(arg.substr(12).c_str());
            } else if (arg == "--min-depth") {
                if (i + 1 < argc) minDepth = std::atoi(argv[++i]);
            } else if (arg.rfind("--min-depth=", 0) == 0) {
                minDepth = std::atoi(arg.substr(12).c_str());
            } else if (arg == "-e" || arg == "--extension") {
                if (i + 1 < argc) extensions.push_back(argv[++i]);
            } else if (arg == "-t" || arg == "--type") {
                if (i + 1 < argc) {
                    std::string t = argv[++i];
                    if (t == "f" || t == "file") fileTypes.push_back('f');
                    else if (t == "d" || t == "directory") fileTypes.push_back('d');
                    else if (t == "l" || t == "symlink") fileTypes.push_back('l');
                    else if (t == "x" || t == "executable") fileTypes.push_back('x');
                }
            } else if (arg == "-x" || arg == "--exec") {
                if (i + 1 < argc) {
                    execCommand = argv[++i];
                }
            } else if (arg == "-c" || arg == "--color") {
                if (i + 1 < argc) {
                    std::string val = argv[++i];
                    if (val == "never") {
                        colorOutput = false;
                        colorOutputExplicit = true;
                    } else if (val == "always") {
                        colorOutput = true;
                        colorOutputExplicit = true;
                    }
                }
            } else if (arg[0] == '-' && arg.length() > 1) {
                std::cerr << Color::RED << "Error: Unknown option '" << arg << "'" << Color::RESET << "\n";
                std::cerr << "For usage information, run: fd --help\n";
                return false;
            } else {
                searchPaths.push_back(arg);
            }
        }

        if (!searchPaths.empty()) {
            if (searchPaths.size() == 1 && fs::exists(searchPaths[0]) && fs::is_directory(searchPaths[0])) {
                // Single directory path
            } else {
                pattern = searchPaths.front();
                searchPaths.erase(searchPaths.begin());
            }
        }

        if (searchPaths.empty()) {
            searchPaths.push_back(".");
        }

        if (!colorOutputExplicit && !ConsoleEnvironment::ShouldUseColorByDefault()) {
            colorOutput = false;
        }

        return true;
    }

    void PrintHelp() const {
        std::cout << R"(fd(1)                   CrossShell for UNIX Reference Manual                    fd(1)

    NAME
        fd - simple, fast and user-friendly alternative to find

    SYNOPSIS
        fd [OPTIONS] [PATTERN] [PATH...]

    DESCRIPTION
        fd is a fast, intuitive search tool for finding filesystem entries matching
        regular expressions or globs, respecting gitignore and hidden files by default.

    OPTIONS
        -H, --hidden
            Search hidden files and directories.

        -I, --no-ignore
            Do not respect .(git)ignore files.

        -s, --case-sensitive
            Case-sensitive search (default is smart case).

        -t, --type TYPE
            Filter by entry type: f (file), d (directory), l (symlink), e (empty).

        -e, --extension EXT
            Filter by file extension.

        -x, --exec COMMAND
            Execute a command for each search result.

        --json, --csv, --table
            Format search results as JSON, CSV, or table.

        --pipe COMMAND
            Pipe matches directly to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        fd "\.cpp$" src/
            Find all C++ source files in src/.

        fd -e pdf --json
            Find all PDF files and emit JSON list.

    CrossShell for UNIX                                                     fd(1)
)";
    }

    void PrintVersion() const {
        std::cout << Color::BOLD << Color::CYAN << "fd" << Color::RESET << " version 1.1.0\n";
    }
};
