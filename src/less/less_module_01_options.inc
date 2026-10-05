// ============================================================================
// 1. OPTIONS & KEY DEFINITIONS
// ============================================================================

enum class KeyCode {
    NONE,
    UP,
    DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END,
    QUIT,
    HELP,
    SEARCH_FWD,
    SEARCH_BWD,
    SEARCH_NEXT,
    SEARCH_PREV,
    TOGGLE_NUMS,
    NEXT_FILE,
    PREV_FILE,
    OTHER
};

class LessOptions {
public:
    bool showLineNumbers{false};
    bool quiet{false};
    std::vector<std::string> files;

    static void printHelp() {
        std::cout << R"(less(1)             CrossShell for UNIX Reference Manual                 less(1)

    NAME
        less - terminal pager for viewing text files interactively

    SYNOPSIS
        less [OPTIONS] [FILE...]

    DESCRIPTION
        less is a terminal paging program that allows forward and backward
        navigation through a file or standard input. It does not read the entire
        input before starting, allowing faster startup with large files.

    OPTIONS
        -N, --LINE-NUMBERS
            Display line numbers preceding each line.

        -q, --quiet
            Operate in quiet mode with minimal alerts.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    NAVIGATION CONTROLS
        j, Down, Enter
            Scroll forward one line.

        k, Up, y
            Scroll backward one line.

        f, Space, PgDn
            Scroll forward one window.

        b, PgUp
            Scroll backward one window.

        g, Home
            Navigate to beginning of file.

        G, End
            Navigate to end of file.

        /pattern
            Search forward for pattern.

        ?pattern
            Search backward for pattern.

        n
            Repeat previous search in forward direction.

        N
            Repeat previous search in backward direction.

        q, Q
            Exit the pager.

    EXAMPLES
        less document.txt
            View document.txt interactively.

        less -N source.cpp
            View source.cpp with line numbers.

        git log | less
            Page through piped command output.

    CrossShell for UNIX                                                    less(1)
)";
    }

    static void printVersion() {
        std::cout << "less 1.0\n";
    }

    static bool parse(int argc, char* argv[], LessOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp();
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-N" || arg == "--LINE-NUMBERS") {
                opts.showLineNumbers = true;
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
            } else if (arg[0] == '-' && arg != "-") {
                std::cerr << "less: unrecognized option: " << arg << "\n";
                return false;
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

