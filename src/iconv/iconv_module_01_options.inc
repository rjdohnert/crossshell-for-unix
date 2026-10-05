// ============================================================================
// 1. OPTIONS & CODEPAGE REGISTRY
// ============================================================================

constexpr UINT CP_PSEUDO_UTF16LE = 1200;
constexpr UINT CP_PSEUDO_UTF16BE = 1201;
constexpr UINT CP_PSEUDO_UTF32LE = 12000;
constexpr UINT CP_PSEUDO_UTF32BE = 12001;

class IconvOptions {
public:
    std::string fromCode;
    std::string toCode;
    std::string outputFile;
    std::vector<std::string> inputFiles;
    bool discardInvalid{false}; // -c / //IGNORE
    bool translit{false};        // //TRANSLIT
    bool silent{false};          // -s
    bool list{false};            // -l
    int outputFormat{0};
    std::string pipeCommand;

    static void printHelp() {
        std::cout << R"(iconv(1)            CrossShell for UNIX Reference Manual                 iconv(1)

    NAME
        iconv - convert text character encoding

    SYNOPSIS
        iconv [OPTIONS] [-f FROM_ENCODING] [-t TO_ENCODING] [FILE...]
        iconv -l

    DESCRIPTION
        iconv converts text from one character set encoding to another. If no
        input files are given, or if FILE is '-', standard input is read. Output
        is written to standard output unless specified otherwise by -o.

    OPTIONS
        -f, --from-code FROM
            Specify the input character encoding.

        -t, --to-code TO
            Specify the output character encoding.

        -c
            Discard characters that cannot be converted instead of terminating.

        -s, --silent
            Suppress conversion error warning messages.

        -l, --list
            List all supported character set encodings.

        -o, --output FILE
            Write converted output to FILE instead of standard output.

        --json
            Output results formatted as JSON.

        --csv
            Output results formatted as CSV.

        --table
            Output results formatted as a table.

        --pipe COMMAND
            Send output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        iconv -f UTF-8 -t UTF-16 input.txt -o output.txt
            Convert input.txt from UTF-8 to UTF-16 and write to output.txt.

        iconv -l
            List all supported character encodings.

        type file.txt | iconv -f ISO-8859-1 -t UTF-8
            Convert standard input stream to UTF-8.

    CrossShell for UNIX                                                    iconv(1)
)";
    }

    static void printVersion() {
        std::cout << "iconv v2.2.0\n";
    }

    static bool parse(int argc, char* argv[], IconvOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printHelp();
                std::exit(0);
            } else if (arg == "--version" || arg == "-V") {
                printVersion();
                std::exit(0);
            } else if (arg == "-l" || arg == "--list") {
                opts.list = true;
            } else if (arg == "-c") {
                opts.discardInvalid = true;
            } else if (arg == "-s" || arg == "--silent") {
                opts.silent = true;
            } else if (arg.rfind("-f", 0) == 0) {
                opts.fromCode = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            } else if (arg.rfind("--from-code=", 0) == 0) {
                opts.fromCode = arg.substr(12);
            } else if (arg.rfind("-t", 0) == 0) {
                opts.toCode = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            } else if (arg.rfind("--to-code=", 0) == 0) {
                opts.toCode = arg.substr(10);
            } else if (arg.rfind("-o", 0) == 0) {
                opts.outputFile = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            } else if (arg.rfind("--output=", 0) == 0) {
                opts.outputFile = arg.substr(9);
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::cerr << "iconv: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                opts.inputFiles.push_back(arg);
            }
        }

        if (opts.inputFiles.empty() && !opts.list) {
            opts.inputFiles.push_back("-");
        }

        return true;
    }
};

class EncodingRegistry {
public:
    static std::string normalize(std::string_view name) {
        std::string norm;
        norm.reserve(name.size());
        for (char c : name) {
            if (c != '-' && c != '_' && c != ' ' && c != ':') {
                norm.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
        }
        return norm;
    }

    static UINT lookupCodePage(const std::string& name) {
        static const std::unordered_map<std::string, UINT> registry = {
            {"utf8", CP_UTF8}, {"utf8bom", CP_UTF8}, {"utf7", CP_UTF7},
            {"utf16", CP_PSEUDO_UTF16LE}, {"utf16le", CP_PSEUDO_UTF16LE}, {"utf16be", CP_PSEUDO_UTF16BE},
            {"ucs2", CP_PSEUDO_UTF16LE}, {"ucs2le", CP_PSEUDO_UTF16LE}, {"ucs2be", CP_PSEUDO_UTF16BE},
            {"utf32", CP_PSEUDO_UTF32LE}, {"utf32le", CP_PSEUDO_UTF32LE}, {"utf32be", CP_PSEUDO_UTF32BE},
            {"ascii", 20127}, {"usascii", 20127},
            {"latin1", 28591}, {"iso88591", 28591}, {"iso88592", 28592}, {"iso885915", 28605},
            {"windows1250", 1250}, {"cp1250", 1250},
            {"windows1251", 1251}, {"cp1251", 1251},
            {"windows1252", 1252}, {"cp1252", 1252},
            {"shiftjis", 932}, {"sjis", 932}, {"cp932", 932},
            {"gbk", 936}, {"cp936", 936}, {"gb2312", 936},
            {"euckr", 949}, {"cp949", 949},
            {"big5", 950}, {"cp950", 950}
        };

        std::string n = normalize(name);
        auto it = registry.find(n);
        if (it != registry.end()) return it->second;

        // Try direct codepage integer
        try {
            return static_cast<UINT>(std::stoul(name));
        } catch (...) {
            return 0;
        }
    }

    static void listSupported() {
        std::cout << "The following encodings are supported:\n"
                  << "  UTF-8, UTF-16, UTF-16LE, UTF-16BE, UTF-32, UTF-32LE, UTF-32BE\n"
                  << "  ASCII, ISO-8859-1, ISO-8859-2, ISO-8859-15\n"
                  << "  CP1250, CP1251, CP1252 (Windows ANSI)\n"
                  << "  Shift-JIS (CP932), GBK/GB2312 (CP936), EUC-KR (CP949), Big5 (CP950)\n";
    }
};

