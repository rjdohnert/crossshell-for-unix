// Command-Line Interface & Application Controller
// ============================================================================
struct Options {
    SignalInfo signal{ 15, "SIGTERM", "Terminate", SignalType::SIGTERM };
    bool force = false;
    bool tree = false;
    bool verbose = false;
    bool listSignals = false;
    bool listTable = false;
    std::vector<std::string> targets;
};

class CommandLineParser {
public:
    static void printHelp(const std::string& /*exeName*/ = "kill") {
        std::cout << R"(kill(1)             CrossShell for UNIX Reference Manual                 kill(1)

    NAME
        kill - send signals to Windows processes

    SYNOPSIS
        kill [-s SIGNAL | -SIGNAL] [OPTIONS] TARGET...
        kill -l [SIGNAL]
        kill -L

    DESCRIPTION
        kill sends signals to Windows processes identified by process ID (PID) or
        executable image name. It provides UNIX-compatible signaling semantics
        adapted to Windows process APIs, including graceful termination (WM_CLOSE),
        hard termination (TerminateProcess), and process suspension/resumption.
        When no targets are supplied, kill reads PIDs from standard input.

    OPTIONS
        -s SIGNAL
            Send specified signal by name or number (e.g., 9, KILL, 15, TERM).

        -SIGNAL
            Shorthand signal specification (e.g., -9, -15, -KILL, -TERM, -STOP).

        -f, --force
            Force immediate termination and skip graceful closing attempts.

        -t, --tree
            Terminate target process and all child processes recursively.

        -v, --verbose
            Display detailed execution diagnostics.

        -l, --list
            List available signal names or translate numeric signal code.

        -L, --table
            Display formatted table of supported signals.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    SUPPORTED SIGNALS
        0, SIGNULL
            Check process existence without sending a signal.

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
        kill 1234
            Send SIGTERM to process with PID 1234.

        kill -9 notepad.exe
            Forcefully terminate all running notepad.exe processes.

        kill -STOP 4820
            Freeze process execution on PID 4820.

        kill -CONT 4820
            Resume execution on PID 4820.

        tasklist | awk '{print $2}' | kill -9
            Pipe PIDs directly into kill.

    CrossShell for UNIX                                                    kill(1)
)";
    }

    static void printVersion() {
        std::cout << "kill 1.0.0\n";
    }

    static Options parse(int argc, char* argv[], const SignalRegistry& registry) {
        Options opt;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp("kill");
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-l" || arg == "--list") {
                opt.listSignals = true;
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    opt.targets.push_back(argv[++i]);
                }
            } else if (arg == "-L" || arg == "--table") {
                opt.listTable = true;
            } else if (arg == "-f" || arg == "--force") {
                opt.force = true;
            } else if (arg == "-t" || arg == "--tree") {
                opt.tree = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opt.verbose = true;
            } else if (arg == "-s") {
                if (i + 1 >= argc) {
                    std::cerr << "kill: Option -s requires a signal argument.\n";
                    std::exit(1);
                }
                auto found = registry.find(argv[++i]);
                if (!found) {
                    std::cerr << "kill: Unknown signal '" << argv[i] << "'. Use -l for a list.\n";
                    std::exit(1);
                }
                opt.signal = *found;
            } else if (arg.front() == '-' && arg.size() > 1 && !std::isdigit(static_cast<unsigned char>(arg[1])) && registry.find(arg.substr(1))) {
                opt.signal = *registry.find(arg.substr(1));
            } else if (arg.front() == '-' && arg.size() > 1 && std::isdigit(static_cast<unsigned char>(arg[1]))) {
                auto found = registry.find(arg.substr(1));
                if (found) {
                    opt.signal = *found;
                } else {
                    std::cerr << "kill: Invalid signal specification: " << arg << "\n";
                    std::exit(1);
                }
            } else if (arg.front() == '-') {
                std::cerr << "kill: Unknown flag '" << arg << "'. Use -h for help.\n";
                std::exit(1);
            } else {
                opt.targets.push_back(arg);
            }
        }

        return opt;
    }
};

