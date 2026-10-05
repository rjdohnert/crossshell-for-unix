// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

class CalOptions {
public:
    bool julian{false};
    bool mondayFirst{false};
    int month{-1};
    int year{-1};
    bool fullYear{false};
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* exe = nullptr) {
        (void)exe;
        std::cout << R"(cal(1)                  CrossShell for UNIX Reference Manual                  cal(1)

    NAME
        cal - display a calendar

    SYNOPSIS
        cal [OPTIONS] [MONTH] [YEAR]

    DESCRIPTION
        Display a calendar for the specified month or year. If no arguments
        are specified, the current month is displayed.

    OPTIONS
        -j, --julian
            Display Julian dates (days numbered 1 to 365/366).

        -m, --monday
            Display Monday as the first day of the week.

        -y, --year
            Display a calendar for the entire specified or current year.

        --json, --csv, --table
            Output calendar metadata in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send formatted output directly through COMMAND.

        -h, --help
            Display this reference manual.

        -v, --version
            Display version and license information.

    EXAMPLES
        cal
            Display current month calendar.

        cal 12 2026
            Display calendar for December 2026.

        cal -y
            Display full calendar for the current year.

        cal --json
            Display current month metadata in JSON format.

    CrossShell for UNIX                                                    cal(1)
)";
    }

    static void printVersion() {
        std::cout << "cal 2.0.0\n";
    }

    static int parseMonth(const std::string& text) {
        if (text.empty()) return -1;
        try {
            int m = std::stoi(text);
            if (m >= 1 && m <= 12) return m;
        } catch (...) {}
        return -1;
    }

    static int parseYear(const std::string& text) {
        if (text.empty()) return -1;
        try {
            int y = std::stoi(text);
            if (y >= 1 && y <= 9999) return y;
        } catch (...) {}
        return -1;
    }

    static bool parse(int argc, char* argv[], CalOptions& opts) {
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "-v" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-j" || arg == "--julian") {
                opts.julian = true;
            } else if (arg == "-m" || arg == "--monday") {
                opts.mondayFirst = true;
            } else if (arg == "-y" || arg == "--year") {
                opts.fullYear = true;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::cerr << "cal: invalid option: " << arg << "\n";
                return false;
            } else {
                positional.push_back(arg);
            }
        }

        std::time_t t = std::time(nullptr);
        std::tm tmv{};
        localtime_s(&tmv, &t);
        int curMonth = tmv.tm_mon + 1;
        int curYear = tmv.tm_year + 1900;

        if (positional.empty()) {
            if (opts.fullYear) {
                opts.month = 0;
                opts.year = curYear;
            } else {
                opts.month = curMonth;
                opts.year = curYear;
            }
        } else if (positional.size() == 1) {
            int y = parseYear(positional[0]);
            if (y > 0) {
                opts.month = 0;
                opts.year = y;
                opts.fullYear = true;
            } else {
                int m = parseMonth(positional[0]);
                if (m > 0) {
                    opts.month = m;
                    opts.year = curYear;
                } else {
                    std::cerr << "cal: illegal argument: " << positional[0] << "\n";
                    return false;
                }
            }
        } else if (positional.size() >= 2) {
            opts.month = parseMonth(positional[0]);
            opts.year = parseYear(positional[1]);
            if (opts.month < 1 || opts.year < 1) {
                std::cerr << "cal: illegal month/year arguments\n";
                return false;
            }
        }

        return true;
    }
};

