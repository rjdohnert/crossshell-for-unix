#include "seq_options.hpp"
#include "number_parser.hpp"

void SeqOptions::printUsage(const char* progName) {
    (void)progName;
    std::cout << R"(seq(1)                  CrossShell for UNIX Reference Manual                       seq(1)

    NAME
        seq - print a numeric sequence

    SYNOPSIS
        seq [OPTIONS] LAST
        seq [OPTIONS] FIRST LAST
        seq [OPTIONS] FIRST INCREMENT LAST

    DESCRIPTION
        Prints numbers from FIRST through LAST using INCREMENT. Negative numeric
        values are accepted as positional values, and a direction mismatch produces
        no sequence with a successful exit status.

    OPTIONS
        -f, --format FORMAT
            Use a printf-style floating-point format.

        -s, --separator STRING
            Set the separator; default is newline.

        -w, --equal-width
            Pad values with leading zeroes.

        -h, --help, /?
            Display this reference manual.

        --version
            Display version information and exit.

        --
            End options before numeric operands.

    EXAMPLES
        seq 5
            Print sequence from 1 to 5.

        seq 2 5
            Print sequence from 2 to 5.

        seq 1 0.5 3
            Print sequence with 0.5 increment.

        seq -s, 1 3
            Print comma-separated sequence.

        seq -w 8 10
            Print zero-padded sequence.

    EXIT STATUS
        0          Help, version, direction mismatch, or successful generation.
        1          Invalid option/number, missing argument, or zero increment.

    CrossShell for UNIX                                                          seq(1)
)";
}

void SeqOptions::printVersion() {
    std::cout << "seq 1.0\n";
}

bool SeqOptions::parse(int argc, char* argv[], SeqOptions& outOpts) {
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--") {
            for (++i; i < argc; ++i) {
                positional.push_back(argv[i]);
            }
            break;
        } else if (arg == "-h" || arg == "--help" || arg == "/?") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--version") {
            printVersion();
            std::exit(0);
        } else if (arg == "-s" || arg == "--separator") {
            if (i + 1 < argc) outOpts.separator = NumberParser::unescape(argv[++i]);
            else { std::cerr << "seq: option requires an argument -- 's'\n"; return false; }
        } else if (arg.rfind("--separator=", 0) == 0) {
            outOpts.separator = NumberParser::unescape(arg.substr(12));
        } else if (arg.rfind("-s", 0) == 0 && arg.size() > 2) {
            outOpts.separator = NumberParser::unescape(arg.substr(2));
        } else if (arg == "-f" || arg == "--format") {
            if (i + 1 < argc) outOpts.customFormat = argv[++i];
            else { std::cerr << "seq: option requires an argument -- 'f'\n"; return false; }
        } else if (arg.rfind("--format=", 0) == 0) {
            outOpts.customFormat = arg.substr(9);
        } else if (arg.rfind("-f", 0) == 0 && arg.size() > 2) {
            outOpts.customFormat = arg.substr(2);
        } else if (arg == "-w" || arg == "--equal-width") {
            outOpts.equalWidth = true;
        } else if (!arg.empty() && arg[0] == '-' && arg.length() > 1 && (std::isdigit(static_cast<unsigned char>(arg[1])) || arg[1] == '.')) {
            positional.push_back(arg);
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "seq: invalid option '" << arg << "'\n";
            return false;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.empty() || positional.size() > 3) {
        std::cerr << "seq: invalid number of arguments\n";
        std::cerr << "Try '" << argv[0] << " --help' for more information.\n";
        return false;
    }

    if (positional.size() == 1) {
        outOpts.startStr = "1";
        outOpts.incrementStr = "1";
        outOpts.lastStr = positional[0];
    } else if (positional.size() == 2) {
        outOpts.startStr = positional[0];
        outOpts.incrementStr = "1";
        outOpts.lastStr = positional[1];
    } else {
        outOpts.startStr = positional[0];
        outOpts.incrementStr = positional[1];
        outOpts.lastStr = positional[2];
    }

    return true;
}
