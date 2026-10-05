    // =========================================================================
    // Command-Line Argument Parser
    // =========================================================================
    class ArgumentParser {
    public:
        static Options parse(int argc, char* argv[]) {
            Options options;

            for (int i = 1; i < argc; ++i) {
                std::string_view arg = argv[i];

                if (arg == "-h" || arg == "--help" || arg == "/?") {
                    options.show_help = true;
                    return options;
                }
                if (arg == "-v" || arg == "-V" || arg == "--version") {
                    options.show_version = true;
                    return options;
                }
                if (arg == "--json") { options.output_format = 1; continue; }
                if (arg == "--csv") { options.output_format = 2; continue; }
                if (arg == "--table") { options.output_format = 3; continue; }
                if (arg == "--pipe" && i + 1 < argc) { options.pipe_command = argv[++i]; continue; }
                if (arg == "-b" || arg == "--bytes") {
                    options.count_bytes = true;
                }
                else if (arg == "-s" || arg == "--spaces") {
                    options.break_at_spaces = true;
                }
                else if (arg == "-w" || arg == "--width") {
                    if (i + 1 < argc) {
                        options.width = parse_width(argv[++i]);
                    } else {
                        throw std::runtime_error("Option requires an argument: " + std::string(arg));
                    }
                }
                else if (arg.rfind("--width=", 0) == 0) {
                    options.width = parse_width(arg.substr(8));
                }
                else if (arg.rfind("-w", 0) == 0 && arg.size() > 2) {
                    options.width = parse_width(arg.substr(2));
                }
                else if (arg.size() > 1 && arg[0] == '-' && std::isdigit(static_cast<unsigned char>(arg[1]))) {
                    // Support legacy syntax like "-80"
                    options.width = parse_width(arg.substr(1));
                }
                else if (arg.size() > 1 && arg[0] == '-' && arg[1] != '-') {
                    // Parse bundled short flags (e.g. -sbw 50)
                    for (size_t j = 1; j < arg.size(); ++j) {
                        char flag = arg[j];
                        if (flag == 'b') {
                            options.count_bytes = true;
                        } else if (flag == 's') {
                            options.break_at_spaces = true;
                        } else if (flag == 'w') {
                            if (j + 1 < arg.size()) {
                                options.width = parse_width(arg.substr(j + 1));
                                break;
                            } else if (i + 1 < argc) {
                                options.width = parse_width(argv[++i]);
                                break;
                            } else {
                                throw std::runtime_error("Option -w requires an argument.");
                            }
                        } else {
                            throw std::runtime_error(std::string("Unknown flag: -") + flag);
                        }
                    }
                }
                else if (arg == "--") {
                    // Treat remainder as filenames
                    for (++i; i < argc; ++i) {
                        options.files.emplace_back(argv[i]);
                    }
                    break;
                }
                else {
                    options.files.emplace_back(arg);
                }
            }

            return options;
        }

        static void print_help(std::ostream& out) {
            out << R"(fold(1)             CrossShell for UNIX Reference Manual                 fold(1)

    NAME
        fold - wrap each input line to fit in specified width

    SYNOPSIS
        fold [OPTIONS] [FILE...]

    DESCRIPTION
        fold wraps input lines in each FILE (or standard input if no file is specified
        or when FILE is '-'), writing to standard output. It integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -b, --bytes
            Count bytes rather than display column positions.

        -s, --spaces
            Break lines at word boundaries (spaces/tabs) within the line width.

        -w, --width WIDTH
            Use specified column or byte width instead of the default 80.

        --json
            Emit folded lines as JSON records.

        --csv
            Emit folded lines as CSV records.

        --table
            Emit folded lines in a table format.

        --pipe COMMAND
            Send formatted output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        fold -w 60 document.txt
            Wrap lines in document.txt to 60 columns.

        fold -s -w 72 long_text.txt > formatted.txt
            Wrap at 72 columns breaking only at spaces.

        type binary_dump.txt | fold -b -w 64
            Wrap byte-wise at 64 bytes per line.

    CrossShell for UNIX                                                    fold(1)
)";
        }

        static void print_version(std::ostream& out) {
            out << "fold 6.0.2\n"
                << "Copyright (C) 2026, Roberto J Dohnert.\n";
        }

    private:
        static size_t parse_width(std::string_view str) {
            try {
                size_t pos = 0;
                unsigned long val = std::stoul(std::string(str), &pos);
                if (pos != str.size() || val == 0) {
                    throw std::invalid_argument("");
                }
                return static_cast<size_t>(val);
            } catch (...) {
                throw std::runtime_error("Invalid width specification: '" + std::string(str) + "'. Width must be a positive integer.");
            }
        }
    };
