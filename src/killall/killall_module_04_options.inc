// ============================================================================
// Command-Line Interface & Configuration
// ============================================================================
struct Options {
    SignalInfo signal{ 15, "SIGTERM", "Terminate", SignalType::SIGTERM };
    bool force = false;
    bool tree = false;
    bool interactive = false;
    bool ignoreCase = true;
    bool exactCase = false;
    bool quiet = false;
    bool verbose = false;
    bool wait = false;
    bool useRegex = false;
    bool listSignals = false;
    bool listTable = false;
    std::string userFilter;
    std::optional<std::chrono::seconds> olderThan;
    std::optional<std::chrono::seconds> youngerThan;
    std::vector<std::string> patterns;
};

class CommandLineParser {
public:
    static void printHelp(const std::string& /*exeName*/ = "killall") {
        std::cout << R"(killall(1)          CrossShell for UNIX Reference Manual                killall(1)

    NAME
        killall - kill processes by name

    SYNOPSIS
        killall [OPTIONS] [-s SIGNAL | -SIGNAL] PATTERN...
        killall -l
        killall -L

    DESCRIPTION
        killall sends a signal to all processes running any of the specified
        commands or patterns. When no signal name is specified, SIGTERM is sent.
        Signals can be specified by name or number. It supports case-insensitive
        matching, regex pattern evaluation, process age filtering, and process trees.

    OPTIONS
        -s SIGNAL
            Specify the signal to send by name or number.

        -SIGNAL
            Shorthand signal specification (e.g., -9, -15, -KILL, -TERM, -STOP).

        -e, --exact
            Require exact case matching of process names.

        -I, --ignore-case
            Use case-insensitive matching (Windows default).

        -i, --interactive
            Interactively ask for confirmation before signaling each process.

        -r, --regexp
            Interpret patterns as regular expressions.

        -t, --tree
            Include target processes and all child processes recursively.

        -u, --user USERNAME
            Match only processes owned by the specified user.

        -o, --older-than DURATION
            Match only processes older than DURATION (e.g., 30m, 2h).

        -y, --younger-than DURATION
            Match only processes younger than DURATION.

        -w, --wait
            Wait for all signaled processes to terminate before exiting.

        -q, --quiet
            Do not complain if no matching processes are found.

        -v, --verbose
            Display detailed execution diagnostics.

        -l, --list
            List all known signal names.

        -L, --table
            Display formatted table of supported signals.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    SUPPORTED SIGNALS
        0, SIGNULL
            Probe process existence and query access.

        1, SIGHUP
            Request graceful window closure.

        2, SIGINT
            Send console interrupt (Ctrl+C).

        3, SIGQUIT
            Send console break (Ctrl+Break).

        9, SIGKILL
            Hard termination via TerminateProcess.

        15, SIGTERM
            Graceful termination with fallback to hard termination.

        18, SIGCONT
            Resume suspended process threads.

        19, SIGSTOP
            Freeze process threads.

    EXAMPLES
        killall notepad.exe
            Send SIGTERM to all notepad.exe instances.

        killall -9 chrome.exe msedge.exe
            Forcefully terminate all Chrome and Edge instances.

        killall -r -i "node.*\.exe"
            Interactively terminate processes matching regex.

        killall -9 -o 30m cl.exe gcc.exe
            Kill compiler workers older than 30 minutes.

        killall -STOP SearchIndexer.exe
            Freeze background indexing process.

    CrossShell for UNIX                                                   killall(1)
)";
    }

    static void printVersion() {
        std::cout << "killall 1.0.0\n";
    }

    static Options parse(int argc, char* argv[], const SignalRegistry& registry) {
        Options opt;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp("killall");
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-l" || arg == "--list") {
                opt.listSignals = true;
            } else if (arg == "-L" || arg == "--table") {
                opt.listTable = true;
            } else if (arg == "-e" || arg == "--exact") {
                opt.exactCase = true;
                opt.ignoreCase = false;
            } else if (arg == "-I" || arg == "--ignore-case") {
                opt.ignoreCase = true;
                opt.exactCase = false;
            } else if (arg == "-i" || arg == "--interactive") {
                opt.interactive = true;
            } else if (arg == "-r" || arg == "--regexp") {
                opt.useRegex = true;
            } else if (arg == "-t" || arg == "--tree") {
                opt.tree = true;
            } else if (arg == "-w" || arg == "--wait") {
                opt.wait = true;
            } else if (arg == "-q" || arg == "--quiet") {
                opt.quiet = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opt.verbose = true;
            } else if (arg == "-u" || arg == "--user") {
                if (i + 1 >= argc) {
                    std::cerr << "killall: Option -u requires a username.\n";
                    std::exit(1);
                }
                opt.userFilter = argv[++i];
            } else if (arg == "-o" || arg == "--older-than") {
                if (i + 1 >= argc) {
                    std::cerr << "killall: Option -o requires a duration.\n";
                    std::exit(1);
                }
                opt.olderThan = TimeHelper::parseDuration(argv[++i]);
                if (!opt.olderThan) {
                    std::cerr << "killall: Invalid duration format: " << argv[i] << "\n";
                    std::exit(1);
                }
            } else if (arg == "-y" || arg == "--younger-than") {
                if (i + 1 >= argc) {
                    std::cerr << "killall: Option -y requires a duration.\n";
                    std::exit(1);
                }
                opt.youngerThan = TimeHelper::parseDuration(argv[++i]);
                if (!opt.youngerThan) {
                    std::cerr << "killall: Invalid duration format: " << argv[i] << "\n";
                    std::exit(1);
                }
            } else if (arg == "-s" || arg == "--signal") {
                if (i + 1 >= argc) {
                    std::cerr << "killall: Option -s requires a signal specification.\n";
                    std::exit(1);
                }
                auto found = registry.find(argv[++i]);
                if (!found) {
                    std::cerr << "killall: Unknown signal '" << argv[i] << "'. Use -l for list.\n";
                    std::exit(1);
                }
                opt.signal = *found;
            } else if (arg.front() == '-' && arg.size() > 1 && registry.find(arg.substr(1))) {
                opt.signal = *registry.find(arg.substr(1));
            } else if (arg.front() == '-') {
                std::cerr << "killall: Unknown option '" << arg << "'. Use -h for help.\n";
                std::exit(1);
            } else {
                opt.patterns.push_back(arg);
            }
        }

        return opt;
    }
};

