// ============================================================================
// Command-Line Parser & Help System
// ============================================================================
class CommandLineParser {
public:
    static void printHelp(const std::string& exeName) {
           std::cout << R"(gsar(1)                 CrossShell for UNIX Reference Manual                     gsar(1)

    NAME
        gsar - general binary and text search-and-replace utility

    SYNOPSIS
        )" << exeName << R"( -s<pattern> [-r<pattern>] [OPTIONS] [INFILE...] [OUTFILE]
        type INPUT | )" << exeName << R"( -s<pattern> -r<pattern> > OUTPUT

    DESCRIPTION
        GSAR searches and replaces patterns in binary data and text files. It can
        modify files in place, search arbitrary binary streams, count occurrences,
        display match contexts, or operate as a filter in Windows CMD and PowerShell
        pipelines.

    OPTIONS
        -s<string>
            Search string or pattern (for example, -s"foo" or -s"\x0D\x0A").

        -r[string]
            Replacement string or pattern. Use -r"" to delete matches.

        -i
            Use case-insensitive ASCII byte matching.

        -c
            Count occurrences only; disable output streaming and replacement.

        -b
            Display match byte offsets in hexadecimal.

        -v
            Enable verbose output summarizing files, matches, and actions.

        -x
            Display matched contexts in hexadecimal dump format.

        -f
            Force pipe/filter mode, reading stdin and writing stdout.

        -u
            Create a backup file with the .bak extension before modifying a file.

        -B<ext>
            Set a custom backup extension (for example, -B.old).

        -o<file>
            Explicitly set the target output file.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

    ESCAPE SEQUENCES
        The following escape sequences are supported in -s and -r patterns:

        \a       Bell or alert (0x07)
        \b       Backspace (0x08)
        \e       Escape (0x1B)
        \f       Form feed (0x0C)
        \n       Newline or line feed (0x0A)
        \r       Carriage return (0x0D)
        	       Horizontal tab (0x09)
        \v       Vertical tab (0x0B)
        \\       Literal backslash
        \0OOO    Octal value, up to 3 octal digits
        \xHH     Hexadecimal value, exactly 2 hex digits
        \dDDD    Decimal value, up to 3 decimal digits

    PIPELINE AND REDIRECTION
        When no input files are specified, or when '-' or -f is provided, gsar reads
        from standard input and writes to standard output. Binary modes are enabled
        automatically to prevent Windows CRLF translation from changing the data.

    EXAMPLES
        )" << exeName << R"( -s"\r\n" -r"\n" script.sh
            Replace Windows CRLF with Unix LF in place.

        type unix.txt | )" << exeName << R"( -s"\n" -r"\r\n" > win.txt
            Convert Unix LF to Windows CRLF in a pipeline.

        )" << exeName << R"( -s"\x4D\x5A\x90\x00" -b -x binary.exe
            Search for a binary signature in an executable.

        )" << exeName << R"( -s"database_v1" -r"database_v2" -i -u config.xml
            Replace text case-insensitively and create a backup.

        )" << exeName << R"( -s"ERROR_FAILED" -c application.log
            Count occurrences of a specific pattern.

    CrossShell for UNIX                                                       gsar(1)
    )";
    }

    static Options parse(int argc, char* argv[]) {
        Options opt;
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp(fs::path(argv[0]).stem().string());
                std::exit(0);
            } else if (arg.rfind("-s", 0) == 0) {
                std::string pat = arg.substr(2);
                if (pat.empty() && i + 1 < argc && argv[i + 1][0] != '-') {
                    pat = argv[++i];
                }
                opt.searchPattern = PatternParser::parse(pat);
                opt.hasSearch = true;
            } else if (arg.rfind("-r", 0) == 0) {
                std::string pat = arg.substr(2);
                if (pat.empty() && i + 1 < argc && argv[i + 1][0] != '-') {
                    pat = argv[++i];
                }
                opt.replacePattern = PatternParser::parse(pat);
                opt.hasReplace = true;
            } else if (arg == "-i") {
                opt.caseInsensitive = true;
            } else if (arg == "-c") {
                opt.countOnly = true;
            } else if (arg == "-v") {
                opt.verbose = true;
            } else if (arg == "-b") {
                opt.showOffsets = true;
            } else if (arg == "-x") {
                opt.displayHex = true;
            } else if (arg == "-f") {
                opt.filterPipeMode = true;
            } else if (arg == "-u") {
                opt.createBackup = true;
            } else if (arg.rfind("-B", 0) == 0) {
                opt.createBackup = true;
                opt.backupExtension = arg.substr(2);
                if (opt.backupExtension.empty() && i + 1 < argc) {
                    opt.backupExtension = argv[++i];
                }
            } else if (arg.rfind("-o", 0) == 0) {
                opt.outputFile = arg.substr(2);
                if (opt.outputFile.empty() && i + 1 < argc) {
                    opt.outputFile = argv[++i];
                }
            } else if (arg.front() == '-') {
                std::cerr << "[gsar] Warning: Unknown option ignored: " << arg << "\n";
            } else {
                positional.push_back(arg);
            }
        }

        if (!opt.hasSearch) {
            std::cerr << "[gsar] Error: Search pattern (-s<string>) is required.\n";
            std::cerr << "       Use -h or --help for detailed usage information.\n";
            std::exit(1);
        }

        // Handle positional input/output files
        if (!positional.empty()) {
            if (positional.size() == 2 && opt.outputFile.empty() && !opt.filterPipeMode) {
                opt.inputFiles.push_back(positional[0]);
                opt.outputFile = positional[1];
            } else {
                for (const auto& file : positional) {
                    opt.inputFiles.push_back(file);
                }
            }
        }

        return opt;
    }
};

