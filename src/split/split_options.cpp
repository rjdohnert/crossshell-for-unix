#include "split_options.hpp"

void SplitOptions::printUsage(const char* progName) {
        std::cout << R"(split(1)                CrossShell for UNIX Reference Manual                 split(1)

    NAME
        split - split a file into pieces

    SYNOPSIS
        split [OPTIONS] [INPUT [PREFIX]]

    DESCRIPTION
        Output pieces of INPUT to PREFIXaa, PREFIXab, ...; default SIZE is 1000
        lines, and default PREFIX is 'x'. Reads standard input when INPUT is '-'
        or omitted. Multiplier suffixes (b=512, k=1024, m=1048576, g=1073741824,
        t=1099511627776) are supported for byte counts.

    OPTIONS
        -a, --suffix-length=N
            Use suffixes of length N (default: 2).

        -b, --bytes=SIZE
            Put SIZE bytes per output file.

        -l, --lines=NUMBER
            Put NUMBER lines per output file.

        -n, --number=CHUNKS
            Split into CHUNKS equal output files.

        -d, --numeric-suffixes
            Use numeric suffixes instead of alphabetic.

        -x
            Use hexadecimal suffixes instead of alphabetic.

        --json
            Emit split telemetry in JSON format.

        --csv
            Emit split telemetry in CSV format.

        --table
            Emit split telemetry in tabular format.

        --pipe COMMAND
            Stream status directly into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Display version information and exit.

    EXAMPLES
        split -l 500 largefile.txt chunk_
            Split largefile.txt into 500-line chunks prefixed with chunk_.

        split -b 10m archive.tar part_
            Split archive into 10 MB pieces.

        split -d -a 3 -b 1k data.bin slice_
            Split data into 1 KB files with 3-digit numeric suffixes.

    CrossShell for UNIX                                                    split(1)
    )";
    }

uint64_t SplitOptions::parseSize(const std::string& str) {
        if (str.empty()) return 0;
        uint64_t mult = 1;
        std::string numStr = str;

        char lastChar = static_cast<char>(std::tolower(static_cast<unsigned char>(str.back())));
        if (std::isalpha(static_cast<unsigned char>(lastChar))) {
            numStr = str.substr(0, str.length() - 1);
            switch (lastChar) {
                case 'b': mult = 512; break;
                case 'k': mult = 1024; break;
                case 'm': mult = 1024ULL * 1024ULL; break;
                case 'g': mult = 1024ULL * 1024ULL * 1024ULL; break;
                case 't': mult = 1024ULL * 1024ULL * 1024ULL * 1024ULL; break;
                default: mult = 1; break;
            }
        }
        return std::stoull(numStr) * mult;
    }

bool SplitOptions::parse(int argc, char* argv[], SplitOptions& opts) {
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg.rfind("-a", 0) == 0 && arg.length() > 2) {
                opts.suffixLen = std::stoi(arg.substr(2));
            } else if (arg == "-a" || arg == "--suffix-length") {
                if (i + 1 < argc) opts.suffixLen = std::stoi(argv[++i]);
            } else if (arg.rfind("-b", 0) == 0 && arg.length() > 2) {
                opts.mode = SplitMode::Bytes;
                opts.byteCount = parseSize(arg.substr(2));
            } else if (arg == "-b" || arg == "--bytes") {
                if (i + 1 < argc) {
                    opts.mode = SplitMode::Bytes;
                    opts.byteCount = parseSize(argv[++i]);
                }
            } else if (arg.rfind("-l", 0) == 0 && arg.length() > 2) {
                opts.mode = SplitMode::Lines;
                opts.lineCount = parseSize(arg.substr(2));
            } else if (arg == "-l" || arg == "--lines") {
                if (i + 1 < argc) {
                    opts.mode = SplitMode::Lines;
                    opts.lineCount = parseSize(argv[++i]);
                }
            } else if (arg.rfind("-n", 0) == 0 && arg.length() > 2) {
                opts.mode = SplitMode::Chunks;
                opts.chunkCount = parseSize(arg.substr(2));
            } else if (arg == "-n" || arg == "--number") {
                if (i + 1 < argc) {
                    opts.mode = SplitMode::Chunks;
                    opts.chunkCount = parseSize(argv[++i]);
                }
            } else if (arg == "-d" || arg == "--numeric-suffixes") {
                opts.suffixType = SuffixType::Numeric;
            } else if (arg == "-x") {
                opts.suffixType = SuffixType::Hex;
            } else if (arg == "-") {
                positional.push_back(arg);
            } else if (!arg.empty() && arg[0] == '-') {
                std::cerr << "split: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                positional.push_back(arg);
            }
        }

        if (!positional.empty()) opts.inputFile = positional[0];
        if (positional.size() > 1) opts.prefix = positional[1];

        return true;
    }
